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

#include "free_disc_space.h"

FreeDiscSpace::FreeDiscSpace(SystemInfos::DiscSpace *ds, QObject *parent) : QObject(parent)
{
    MyDiscSpace = ds;
}

void FreeDiscSpace::init(QString cp)
{
    cache_path = cp;
    MyDiscSpace->recalculate();
}

void FreeDiscSpace::setInventoryTable(DB::InventoryTable *value)
{
    MyInventoryTable = value;
}

void FreeDiscSpace::freedSpace(qint64 size_deleted)
{
    bytes_deleted += size_deleted;
}

qint64 FreeDiscSpace::calculateNeededDiscSpaceToFree(qint64 size_new_file)
{
    MyDiscSpace->recalculate();
    qint64 ret = MyDiscSpace->getBytesFree() - size_new_file - 10;

    if (ret >= 0)
        return 0;

    return ret * -1; // when there is need to delete some files is negative so change the sign
}

bool FreeDiscSpace::freeDiscSpace(qint64 size_to_free)
{
    QMap<qint64, QFileInfo> dir_list = sortCacheContentsByLastRead();
    bytes_deleted = 0;
    for (QMap<qint64, QFileInfo>::Iterator i = dir_list.begin(); i != dir_list.end(); ++i)
    {
        deleteEntry(i.value());

        QStringList list;
        list  << "resourceURI" << i.value().absoluteFilePath() + " removed";
        qInfo(ContentManager) << Logger::getInstance().createEventLogMetaData("OBJECT_REMOVED", list);

        if (size_to_free < bytes_deleted)
            break;
    }

    if (size_to_free > bytes_deleted) // if it is not possible to delete target bytes
    {
        QStringList list1;
        list1  << "errorMessage" << " - Only " + QString::number(bytes_deleted) + "bytes from needed" + QString::number(size_to_free) + "bytes could deleted";
        qCritical(ContentManager) << Logger::getInstance().createEventLogMetaData("FAILED_FREEING_SPACE", list1);
        return false;
    }

    QStringList list2;
    list2  << "information" <<  QString::number(bytes_deleted) + " Bytes of old content were deleted";
    qInfo(ContentManager) << Logger::getInstance().createEventLogMetaData("OBJECTS_REMOVED", list2);

    return true;
}

bool FreeDiscSpace::clearPlayerCache()
{
    QMap<qint64, QFileInfo> dir_list = sortCacheContentsByLastRead();
    bytes_deleted = 0;
    for (QMap<qint64, QFileInfo>::Iterator i = dir_list.begin(); i != dir_list.end(); ++i)
    {
        deleteEntry(i.value());
    }

    if (bytes_deleted == 0) // if it is not possible to delete target bytes
    {
        QStringList list1;
        list1  << "errorMessage" << " - Cache could not be cleared";
        qCritical(ContentManager) << Logger::getInstance().createEventLogMetaData("FAILED_FREEING_SPACE", list1);
        return false;
    }

    QStringList list2;
    list2  << "information" << "Cache is cleared" + QString::number(bytes_deleted) + " Bytes were deleted";
    qInfo(ContentManager) << Logger::getInstance().createEventLogMetaData("OBJECTS_REMOVED", list2);

    return true;

}

void FreeDiscSpace::clearWebCache()
{
    // for desktop
    QDir webcache_dir(cache_path + "/QtWebEngine");
    if (webcache_dir.exists())
        webcache_dir.removeRecursively();

    // for Android WebView
    QStringList data_dirs = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
    QDir android_webcache_dir(data_dirs.at(0) + "/../app_webview");
    if (android_webcache_dir.exists())
        android_webcache_dir.removeRecursively();

}

void FreeDiscSpace::deleteEntry(QFileInfo fi)
{
    if (fi.isDir())
    {
        deleteDirectory(fi.absoluteFilePath());
        deleteFile(fi.absoluteFilePath()+".wgt");
    }
    else
    {
        deleteFile(fi.absoluteFilePath());
    }

    // delete entry from db
    if (MyInventoryTable != Q_NULLPTR)
        MyInventoryTable->deleteByCacheName(fi.fileName());

}

bool FreeDiscSpace::deleteFile(QString file_path)
{
    QFile del_file(file_path);

    if (!del_file.exists()) // not an error. to prevent eventually inconsistencies e.g. wgt <=> wgt directory
        return true;

    qint64 del_size = del_file.size();
    if (!del_file.remove())
    {
        qWarning(Develop) << file_path << " cannot be removed " << del_file.errorString();
        return false;
    }
    freedSpace(del_size);
    return true;
}

bool FreeDiscSpace::deleteDirectory(QString dir_path)
{
    QDir del_dir(dir_path);

    if (!del_dir.exists()) // not an error. to prevent eventually inconsistencies e.g wgt <=> wgt directory
        return true;

    quint64 dir_size = calculateDirectorySize(dir_path);
    if (!del_dir.removeRecursively())
    {
        qWarning(Develop) << dir_path << " cannot be removed";
        return false;
    }
    freedSpace(dir_size);
    return true;
}

qint64 FreeDiscSpace::calculateDirectorySize(QString dir_path)
{
    QDirIterator it(dir_path, QDir::Files, QDirIterator::Subdirectories);
    QFileInfo fi;
    qint64 dir_size = 0;
    while (it.hasNext())
    {
        fi.setFile(it.next());
        dir_size += fi.size();
    }
    return dir_size;
}

QMap<qint64, QFileInfo> FreeDiscSpace::sortCacheContentsByLastRead()
{
    QDir dir(cache_path);
    dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot); // do not work, when used in entryInfoList overloaded params directly so set here first
    QFileInfoList dir_list = dir.entryInfoList();

    QMap<qint64, QFileInfo> map;
    for(int i = 0; i < dir_list.size(); i++)
    {
        if (dir_list.at(i).fileName() != "index.smil" && dir_list.at(i).fileName() != "QtWebEngine")
            map.insert(dir_list.at(i).lastRead().toMSecsSinceEpoch(), dir_list.at(i));
    }

    return map;
}

