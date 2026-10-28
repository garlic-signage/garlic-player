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
#ifndef XML_CONFIGURATION_H
#define XML_CONFIGURATION_H

#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QFileInfo>
#include "base_manager.h"

namespace SmilHead
{
    class XMLConfiguration : public Files::BaseManager
    {
        Q_OBJECT
    public:
        explicit XMLConfiguration(DB::InventoryTable *it, MainConfiguration *config, FreeDiscSpace *fds, QObject *parent = nullptr);

        void processFromUrl(QUrl configuration_url);
        void processFromLocalFile(QString file_path);

    protected:
        Downloader        *XMLDownloader;
        QDomDocument       document;
        bool               loadDocument(QString file_path);
        QString            getAttribute(QDomElement element, QString attr_name);
        void               parse();

    protected slots:
        void doSucceed(TNetworkAccess *network);
    signals:
        void finishedConfiguration();

    };
}
#endif // XML_CONFIGURATION_H
