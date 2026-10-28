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
#ifndef TASKSCHEDULER_H
#define TASKSCHEDULER_H

#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QFileInfo>
#include "xml_configuration.h"
#include "firmware_downloader.h"

namespace SmilHead
{
    struct UpdateSettings
    {
        QString task_id          = "";
        QUrl    source_uri       = QUrl();
        QString content_type     = "";
        int     content_length   = 0;
        QString content_checksum = "";
        QString checksum_method  = "MD5";
    };
    struct FirmwareUpdate
    {
        QString task_id          = "";
        QUrl    source_uri       = QUrl();
        QString version          = "";
        QString content_type     = "";
        int     content_length   = 0;
        QString content_checksum = "";
        QString checksum_method  = "MD5";
    };
    struct ApplyCommand
    {
        QString task_id          = "";
        QString command          = "";
    };
    struct ShutdownPlayer
    {
        QString task_id          = "";
        QString content_type     = "reboot";
    };

    class TaskScheduler : public Files::BaseManager
    {
            Q_OBJECT
        public:
            explicit TaskScheduler(DB::InventoryTable *it, MainConfiguration *config, FreeDiscSpace *fds, QObject *parent = nullptr);
            void processFromUrl(QUrl url);

    protected:
            QScopedPointer<SmilHead::XMLConfiguration> MyXMLConfiguration;
            QScopedPointer<SmilHead::FirmwareDownloader> MyFirmwareDownloader;
            Downloader         *TaskFileDownloader;
            DB::InventoryTable *MyInventoryTable;
            FreeDiscSpace      *MyFreeDiscSpace;
            QDomDocument        document;
            UpdateSettings      MyUpdateSetting;
            FirmwareUpdate      MyFirmwareUpdate;
            ShutdownPlayer      MyShutdownPlayer;
            ApplyCommand        MyApplyCommand;
            void            parseFirmwareUpdate(QDomElement element);
            void            parseUpdateSetting(QDomElement element);
            void            parseShutdownPlayer(QDomElement element);
            void            parseApplyCommand(QDomElement update_settings);
            bool            hasUsedTaskId(QString task_id, QString task_name);
            bool            loadDocument(QString file_path);
    protected slots:
            void doSucceed(TNetworkAccess *network);
            void emitApplyConfiguration();
            void emitInstallSoftware(QString file_path);

    signals:
        void                   applyConfiguration();
        void                   installSoftware(QString file_path);
        void                   reboot(QString task_id);
        void                   applyCommand(QString task_id, QString command);
    };
}
#endif // TASKSCHEDULER_H
