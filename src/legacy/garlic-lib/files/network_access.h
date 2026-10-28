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
#ifndef NETWORK_ACCESS_H
#define NETWORK_ACCESS_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QThread>
#include "tools/logger.h"
#include "main_configuration.hpp"

class TNetworkAccess : public QObject
{
        Q_OBJECT
    public:
        explicit TNetworkAccess(MainConfiguration *config, QObject *parent = nullptr);
        // getter/setter
        QByteArray getUserAgent();
        QUrl       getRemoteFileUrl();
        void       setRemoteFileUrl(QUrl value);

    protected:
        MainConfiguration   *MyConfiguration;
        QUrl              remote_file_url;
        QNetworkRequest   prepareConditionalRequest(QUrl remote_url, const QDateTime &lastModifiedUtc, QString etag);
        QNetworkRequest   prepareNetworkRequest(QUrl remote_url);

    signals:
        void              succeed(TNetworkAccess *);
        void              failed(TNetworkAccess *);
};

#endif // NETWORK_ACCESS_H
