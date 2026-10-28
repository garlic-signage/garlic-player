/************************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2016 Nikolaos Saghiadinos <ns@smil-control.com>
    This file is part of the garlic-player source code

    This program is free software: you can redistribute it and/or  modify
    it under the terms of the GNU Affero General Public License, version 3,
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*************************************************************************************/
#include "downloader.h"

Downloader::Downloader(FreeDiscSpace *fds, DB::InventoryTable *it, MainConfiguration *config, QObject *parent) : TNetworkAccess(config, parent)
{
    MyInventoryTable = it;
    MyFreeDiscSpace = fds;
    manager_head.reset(new QNetworkAccessManager(this));
    manager_head.data()->thread()->setPriority(QThread::LowestPriority);
    connect(manager_head.data(), SIGNAL(finished(QNetworkReply*)), SLOT(finishedHeadRequest(QNetworkReply*)));
}

Downloader::~Downloader()
{
    if (!MyFileDownloader.isNull())
         MyFileDownloader.data()->cancelDownload();
}

void Downloader::processFile(QUrl url, QFileInfo fi)
{
    setRemoteFileUrl(url);
    setLocalFileInfo(fi);
    is_request_in_progress = true;
    if (MyInventoryTable != Q_NULLPTR)
    {
        currentDataset = MyInventoryTable->findByCacheName(local_file_info.fileName());
        handleDifferentServer();
    }

    manager_head.data()->head(prepareConditionalRequest(remote_file_url, currentDataset.last_update, currentDataset.etag));
    return;
}

QFileInfo Downloader::getLocalFileInfo()
{
    return local_file_info;
}

void Downloader::finishedHeadRequest(QNetworkReply *reply)
{
    qDebug() << "finished HEAD Request for " << remote_file_url.toString();
    if (reply->error() != QNetworkReply::NoError)
    {
        handleNetworkError(reply);
        reply->deleteLater();
        return;
    }
    int status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status_code != 301 && status_code != 302)
        checkStatusCode(reply, status_code);
    else
    {
        // change remote_file_url with new redirect address
        QUrl remote_file_url_301 = examineRedirectUrl(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toString());
        QNetworkRequest request = prepareConditionalRequest(remote_file_url_301, currentDataset.last_update, currentDataset.etag);

        manager_head_redirect.reset(new QNetworkAccessManager(this));
        manager_head_redirect.data()->thread()->setPriority(QThread::LowestPriority);
        connect(manager_head_redirect.data(), SIGNAL(finished(QNetworkReply*)), SLOT(finishedHeadRedirectRequest(QNetworkReply*)));
        manager_head_redirect.data()->head(request);
    }
    reply->deleteLater();
    return;
}

void Downloader::finishedHeadRedirectRequest(QNetworkReply *reply)
{
    qInfo(Develop) << "finished Redirect Request for " << remote_file_url.toString();
    is_request_in_progress = false;
    if (reply->error() != QNetworkReply::NoError)
    {
        handleNetworkError(reply);
        return;
    }
    checkStatusCode(reply, reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    reply->deleteLater();
    return;
}

void Downloader::checkStatusCode(QNetworkReply *reply, int status_code)
{
    switch (status_code)
    {
        case 200:
                checkHttpHeaders(reply);
                break;
        case 304:
        default:
            qWarning(Develop) << "status code: " << status_code << "not handled for " << remote_file_url.toString();
            emitNotModified(reply);
        break;
    }
    return;
}

void Downloader::checkHttpHeaders(QNetworkReply *reply)
{
    if (reply->hasRawHeader("Content-Type"))
    {
        content_type = reply->rawHeader("Content-Type");
    }

    if (content_type.contains("text/html")  || content_type.contains("application/xhtml+xml"))
    {
        is_request_in_progress = false;
        emit notcacheable(this);
        reply->deleteLater();
        return;
    }


    QStringList list;
    if (!validContentType(content_type) && !validFileExtension())
    {
        list << "resourceURI" << remote_file_url.toString()
             << "errorMessage" << "has an unknown content-type or file type: " + content_type + " extension: " + local_file_info.suffix()
             << "lastCachedLength" << QString::number(local_file_info.size())
             << "lastCachedModifiedTime" << local_file_info.lastModified().toString(Qt::ISODate);

        qCritical(ContentManager) << Logger::getInstance().createEventLogMetaData("FETCH_FAILED",list);
        reply->deleteLater();
        is_request_in_progress = false;
        emit failed(this);
        return;
    }

    if (!reply->hasRawHeader("Last-Modified"))
    {
        qWarning() << remote_file_url.toString() << " Server did not replied with Last-Modified to HEAD request";
    }


    QDateTime remote_last_modified = reply->header(QNetworkRequest::LastModifiedHeader).toDateTime();
    remote_size                    = reply->header(QNetworkRequest::ContentLengthHeader).toInt();

    if (reply->hasRawHeader("etag"))
    {
        remoteEtag = reply->rawHeader("etag").trimmed();

        if (remoteEtag.startsWith('\"') && remoteEtag.endsWith('\"'))
            remoteEtag = remoteEtag.mid(1, remoteEtag.length() - 2);
        else if (remoteEtag.startsWith("W/\"") && remoteEtag.endsWith('\"'))
            remoteEtag = remoteEtag.mid(3, remoteEtag.length() - 4);

        if (currentDataset.etag == remoteEtag)
        {
            qDebug() << remote_file_url.toString() << " no need for update";
            emitNotModified(reply);
            return;
        }
        else
            prepareDownload(reply);

        return;
    }


    // Fallback
    // we need to check for size and last Modified, cause a previous index smil on the server can have a older Date and would not be loaded
    // we need to check also if there is an already downloaded file which is signed and waiting via downloaded suffix
    QFileInfo fi(local_file_info.absoluteFilePath() + FILE_DOWNLOADED_SUFFIX);
    if (
            (local_file_info.exists() && local_file_info.size() == remote_size && local_file_info.lastModified().toUTC() > remote_last_modified)
            ||
            (fi.exists() && fi.size() ==  remote_size && fi.lastModified().toUTC() > remote_last_modified)
        )
    {
        qDebug() << remote_file_url.toString() << " no need for update";
        emitNotModified(reply);
        return;
    }

    prepareDownload(reply);

    return;
}

void Downloader::prepareDownload(QNetworkReply *reply)
{
    if (!canStoreNewFile())
    {
        handleNetworkError(reply);
        return;
    }

    insertDatabase();
    startDownload(reply);
}

void Downloader::startDownload(QNetworkReply *reply)
{
    manager_get.reset(new QNetworkAccessManager(this));
    manager_get.data()->thread()->setPriority(QThread::LowestPriority);
    MyFileDownloader.reset(new FileDownloader(manager_get.data(), MyConfiguration, this));
    connect(MyFileDownloader.data(), SIGNAL(downloadSuccessful()), SLOT(doDownloadSuccessFul()));
    connect(MyFileDownloader.data(), SIGNAL(downloadError(QNetworkReply*)), SLOT(doDownloadError(QNetworkReply*)));

    MyFileDownloader->startDownload(reply->url(), local_file_info.absoluteFilePath(), remote_size, currentDataset.etag);
}

void Downloader::doDownloadSuccessFul()
{
    QStringList list;
    if (!canStoreNewFile())
    {
        QFile::remove(local_file_info.filePath());
        list << "resourceURI" << remote_file_url.toString()
             << "errorMessage" << "Disk quota full cannot store"
             << "transferLength" << QString::number(determineBytesTransfered())
             << "lastCachedLength" << QString::number(local_file_info.size())
             << "lastCachedModifiedTime" << local_file_info.lastModified().toString(Qt::ISODate);
        qWarning(ContentManager) << Logger::getInstance().createEventLogMetaData("FETCH_ERROR",list);

        return;
    }


    list  << "resourceURI" << remote_file_url.toString()
          << "contentLength" << QString::number(local_file_info.size())
          << "lastModifiedTime" << local_file_info.lastModified().toString(Qt::ISODate);

    qInfo(ContentManager) << Logger::getInstance().createEventLogMetaData("OBJECT_UPDATED", list);

    if (MyInventoryTable != Q_NULLPTR)
    {
        MyInventoryTable->updateFileStatusAndSize(remote_file_url.toString(), DB::COMPLETE, local_file_info.size());
    }
    is_request_in_progress = false;

    emit succeed(this);
}

void Downloader::insertDatabase()
{
    if (MyInventoryTable != Q_NULLPTR)
    {
        DB::InventoryDataset saveDataset;
        saveDataset.resource_uri   = remote_file_url.toString();
        saveDataset.cache_name     = local_file_info.fileName();
        saveDataset.content_type   = content_type;
        saveDataset.content_length = remote_size;
        saveDataset.last_update    = QDateTime::currentDateTime();
        saveDataset.etag           = QString::fromUtf8(remoteEtag);
        saveDataset.expires        = QDateTime();
        saveDataset.state          = DB::TRANSFER;
        MyInventoryTable->replace(saveDataset);
    }
}

bool Downloader::canStoreNewFile()
{
    qint64 calc = MyFreeDiscSpace->calculateNeededDiscSpaceToFree(remote_size);
    if (calc > 0 && !MyFreeDiscSpace->freeDiscSpace(calc))
        return false;

    return true;
}

void Downloader::emitNotModified(QNetworkReply *reply)
{
    is_request_in_progress = false;
    emit notmodified(this);
    reply->deleteLater();
}

void Downloader::doDownloadError(QNetworkReply *reply)
{
    handleNetworkError(reply);
}

bool Downloader::isRequestInProgress() const
{
    return is_request_in_progress;
}

bool Downloader::validContentType(QString content_type)
{
    if (!content_type.contains("image/") &&
            !content_type.contains("video/") &&
            !content_type.contains("audio/") &&
            !content_type.contains("text/") &&
            !content_type.contains("application/smil") &&
            !content_type.contains("application/smil+xml") &&
            !content_type.contains("application/xml") &&
            !content_type.contains("application/zip") &&
            !content_type.contains("application/vnd.android.package-archive") &&
            !content_type.contains("application/widget"))
    {
        return false;
    }
    return true;
}

bool Downloader::validFileExtension()
{
    if (local_file_info.suffix() != "wgt" && local_file_info.suffix() != "smil")
    {
        return false;
    }

    return true;

}

void Downloader::handleNetworkError(QNetworkReply *reply)
{
    QStringList list;
    if (local_file_info.exists())
    {
        list << "resourceURI" << remote_file_url.toString()
             << "errorMessage" << reply->errorString()
             << "transferLength" << QString::number(determineBytesTransfered())
             << "lastCachedLength" << QString::number(local_file_info.size())
             << "lastCachedModifiedTime" << local_file_info.lastModified().toString(Qt::ISODate);
        qWarning(ContentManager) << Logger::getInstance().createEventLogMetaData("FETCH_FAILED",list);
    }
    else
    {
        list << "resourceURI" << remote_file_url.toString()
             << "errorMessage" << reply->errorString()
             << "transferLength" << QString::number(determineBytesTransfered());
        qCritical(ContentManager) << Logger::getInstance().createEventLogMetaData("FETCH_FAILED",list);
    }

    qWarning(Develop) << " Download failed " << remote_file_url.toString();
    reply->deleteLater();
    is_request_in_progress = false;
    emit failed(this);
}

QUrl Downloader::examineRedirectUrl(QUrl redirect_url)
{
    if (redirect_url.scheme().isEmpty())
        redirect_url.setScheme(remote_file_url.scheme());
    if (redirect_url.host().isEmpty())
        redirect_url.setHost(remote_file_url.host());
    if (redirect_url.port() == -1)
        redirect_url.setPort(remote_file_url.port());
    return redirect_url;
}

quint64 Downloader::determineBytesTransfered()
{
    quint64 bytes_transfered = 0;
    if (!MyFileDownloader.isNull()) // maybe we get here almost after HEAD and there exists no download attempt
        bytes_transfered = MyFileDownloader.data()->getBytesTransfered();

    return bytes_transfered;
}

void Downloader::handleDifferentServer()
{
    if (currentDataset.etag.isEmpty() && currentDataset.resource_uri != remote_file_url.toString())
    {
        currentDataset.etag = determineLocalFileMd5();
    }

}

QString Downloader::determineLocalFileMd5()
{
    QFile file(local_file_info.absoluteFilePath());
    QByteArray localMd5;
    if (file.open(QIODevice::ReadOnly))
    {
        QCryptographicHash hash(QCryptographicHash::Md5);
        hash.addData(&file );
        localMd5 = hash.result().toHex();
        file.close();
    }

    return QString::fromUtf8(localMd5);
}
