#include "file_downloader.h"

FileDownloader::FileDownloader(QNetworkAccessManager *nam, MainConfiguration *config, QObject* parent) : TNetworkAccess(config, parent), network_manager(nam)
{
}

FileDownloader::~FileDownloader()
{
    //destructor cancels the ongoing dowload (if any)
    if(network_reply)
    {
        cleanupDownload();
    }
}

void FileDownloader::startDownload(QUrl url, QString file_name, qint64 rs, QString etag)
{
    if(network_reply)
        return;

    remote_size        = rs;
    original_file_name = file_name;
    destination_file.setFileName(file_name+FILE_DOWNLOAD_SUFFIX); // add suffix to prevent overwriting
    setRemoteFileUrl(url);

    QFileInfo fi(file_name);
    QNetworkRequest request = prepareConditionalRequest(url, fi.lastModified().toUTC(), etag);
    if (destination_file.exists())
    {
        QByteArray rangeHeaderValue = "bytes=" + QByteArray::number(destination_file.size()) + "-";
        request.setRawHeader("Range",rangeHeaderValue);
        qDebug() << "resume download"  << remote_file_url.toString();
        if(!destination_file.open(QIODevice::Append))
            return;
    }
    else
    {
        if(!destination_file.open(QIODevice::WriteOnly))
            return;
    }

    request.setPriority(QNetworkRequest::LowPriority);

    network_reply = network_manager->get(request);
    connect(network_reply, &QIODevice::readyRead, this, &FileDownloader::readData);
    connect(network_reply, &QNetworkReply::downloadProgress, this, &FileDownloader::downloadProgress);
    connect(network_reply, &QNetworkReply::finished, this, &FileDownloader::finishDownload);
    qDebug() << "started download"  << remote_file_url.toString();
}

void FileDownloader::cancelDownload()
{
    if(!network_reply)
        return;
    cleanupDownload();
    qDebug() << "manually cancelled download"  << remote_file_url.toString();
}

quint64 FileDownloader::getBytesTransfered()
{
    return bytes_transfered;
}

void FileDownloader::addBytesTransfered(quint64 add_bytes )
{
    bytes_transfered = bytes_transfered + add_bytes;
}

void FileDownloader::renameAfterDownload()
{
    QFile original_file(original_file_name);
    if (!original_file.exists())
        destination_file.rename(original_file_name);
    else
        overwriteFile();
}

void FileDownloader::overwriteFile()
{
    // Do not overwrite an existing file, cause it could be possible  that it is loaded.
    // so tag  with ".ready"-suffix and let the media/index-manager decide when to overwrite.

    QString ready_path = original_file_name+FILE_DOWNLOADED_SUFFIX;

    // Qt likes to make life hard and rename is unable to overwrite
    QFile file(ready_path);
    if (file.exists())
        file.remove();

    destination_file.rename(ready_path);
}

void FileDownloader::cleanupDownload()
{
    network_reply->abort();
    network_reply->deleteLater();
    destination_file.close();
    destination_file.remove();
}


void FileDownloader::readData()
{
    addBytesTransfered(destination_file.write(network_reply->readAll()));
}

void FileDownloader::finishDownload()
{
    if(network_reply->error() != QNetworkReply::NoError)
    {
        cleanupDownload();
        emit downloadError(network_reply);
        return;
    }

    readData();
    qint64 tmp = destination_file.size();
    destination_file.close();

    // Some server don't send Content-length or Last-Modified. We can check size only after download
    if (tmp == 0)
    {
        cleanupDownload();
        emit downloadError(network_reply);
        return;
    }

    renameAfterDownload();
    network_reply->deleteLater();

    emit downloadSuccessful();

    qDebug() << "finished download"  << remote_file_url.toString();
}

