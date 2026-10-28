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
#ifndef WEBDAV_H
#define WEBDAV_H

#include <QObject>

#include "files/network_access.h"

class WebDav : public TNetworkAccess
{
        Q_OBJECT
    public:
        explicit WebDav(MainConfiguration *config, QObject *parent = nullptr);
        ~WebDav();
        void       processPutData(QUrl url, QByteArray data);
        QByteArray getSendData() const;
        void       setSendData(const QByteArray value);

    protected:
        QByteArray              send_data;
        QNetworkAccessManager  *manager_put;
        void                    doHttpPutRequest();
        void                    handleNetworkError(QNetworkReply *reply);
    protected slots:
        void finishedPutRequest(QNetworkReply *reply);

};

#endif // WEBDAV_H
