/*************************************************************************************
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
#include "webdav.h"

WebDav::WebDav(MainConfiguration *config, QObject *parent) : TNetworkAccess(config, parent)
{
    manager_put         = new QNetworkAccessManager(this);
    connect(manager_put, SIGNAL(finished(QNetworkReply*)), SLOT(finishedPutRequest(QNetworkReply*)));
}

WebDav::~WebDav()
{
    delete manager_put; // to be clean
}

void WebDav::processPutData(QUrl url, QByteArray data)
{
    setSendData(data);
    setRemoteFileUrl(url);
    doHttpPutRequest();
    return;
}

QByteArray WebDav::getSendData() const
{
    return send_data;
}

void WebDav::setSendData(const QByteArray value)
{
    send_data = value;
}

void WebDav::doHttpPutRequest()
{
    QNetworkRequest request = prepareNetworkRequest(remote_file_url);
    manager_put->clearAccessCache();
    request.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/xml"));
    manager_put->put(request, send_data);
    return;
}


void WebDav::finishedPutRequest(QNetworkReply *reply)
{
     if (reply->error() != QNetworkReply::NoError)
     {
         handleNetworkError(reply);
         return;
     }
     qInfo(Develop) << "SEND_WEBDAV_SUCCEED resourceURI: " << remote_file_url.toString();
     reply->deleteLater();
     emit succeed(this);
}


void WebDav::handleNetworkError(QNetworkReply *reply)
{
   qWarning(Develop) << "SEND_FAILED resourceURI: " << remote_file_url.toString()  << " " << reply->errorString();
   reply->deleteLater();
   emit failed(this);
}
