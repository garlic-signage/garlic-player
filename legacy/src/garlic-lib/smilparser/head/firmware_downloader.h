/*************************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2016 Nikolaos Sagiadinos <ns@smil-control.com>
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
#ifndef FIRMWAREUPDATE_H
#define FIRMWAREUPDATE_H

#include "base_manager.h"

namespace SmilHead
{
    class FirmwareDownloader : public Files::BaseManager
    {
        Q_OBJECT
    public:
        explicit FirmwareDownloader(DB::InventoryTable *it, MainConfiguration *config, FreeDiscSpace *fds, QObject *parent = nullptr);
        void processFromUrl(QUrl firmware_url);

    protected:
        Downloader     *MyDownloader;
        QString         download_file_path = "";
    protected slots:
            void doSucceed(TNetworkAccess *network);
    signals:
            void finishedSoftwareDownload(QString file_path);

    private:
        void removeOldFirmware(QString file_path);
    };
}
#endif // FIRMWAREUPDATE_H
