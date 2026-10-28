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
#ifndef FREE_DISC_SPACE_H
#define FREE_DISC_SPACE_H

#include <QObject>
#include <QStorageInfo>
#include <QDirIterator>
#include "db/inventory_table.h"
#include "QMap"
#include "system_infos/disc_space.hpp"

/**
 * @brief The DiscSpace is a helper class to handle deletes/cleanups for FileModel
 *
 * - calculates the free space of given directory
 * - respect possible quotas by using QStorageInfo::bytesAvailable()
 * -
 * - let an amount of percent of free for OS proposals like log files
 * - deletes files/directories and return deleted bytes
 */
class FreeDiscSpace : public QObject
{
        Q_OBJECT
    public:

        explicit FreeDiscSpace(SystemInfos::DiscSpace *ds, QObject *parent=Q_NULLPTR);
        void     init(QString cp);
        void     freedSpace(qint64 size_deleted);
        bool     freeDiscSpace(qint64 size_to_free);
        bool     clearPlayerCache();
        void     clearWebCache();
        qint64   calculateNeededDiscSpaceToFree(qint64 size);

        void     setInventoryTable(DB::InventoryTable *value);
        bool     deleteFile(QString file_path);

    private:
        void            deleteEntry(QFileInfo fi);
        bool            deleteDirectory(QString dir_path);
        qint64          calculateDirectorySize(QString dir_path);
        void            setBytesTotal(const quint64 &value);
        QMap<qint64, QFileInfo> sortCacheContentsByLastRead();
        DB::InventoryTable     *MyInventoryTable = Q_NULLPTR;
        SystemInfos::DiscSpace *MyDiscSpace;
        qint64         bytes_deleted = 0;
        QString        cache_path = "";
        QFile          del_file;
        QFileInfo      del_file_info;
        QDir           del_dir;


    signals:


};

#endif // FREE_DISC_SPACE_H
