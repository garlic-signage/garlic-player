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
#include "wgt.h"

Wgt::Wgt(QString file_path, QObject *parent) : QObject(parent)
{
    setFilePath(file_path);
}

Wgt::~Wgt()
{
    zip.close();;
}

void Wgt::setFilePath(QString file_path)
{
    local_file_path = file_path;
    zip.setZipName(local_file_path);
    if (!zip.open(QuaZip::mdUnzip))
    {
        qWarning(ContentManager) <<  file_path << " Widget can not be opend. Zip error code: " << zip.getZipError();
    }
}


bool Wgt::isOpen()
{
    return zip.isOpen();
}

QString Wgt::handleRealPath(FreeDiscSpace *MyDiscSpace)
{
    if (!extract(MyDiscSpace))
        return "";
    return local_file_path.mid(0, local_file_path.length()-4)+"/index.html";
}

qint64 Wgt::calculateUncompressedSize()
{
    if (!isOpen())
        return 0;

    quint32 size = 0;
    QList<QuaZipFileInfo> zlist = zip.getFileInfoList();

    QList<QuaZipFileInfo>::iterator i;
    for (i = zlist.begin(); i != zlist.end(); ++i)
    {
        QuaZipFileInfo fi = *i;
        size = size + fi.uncompressedSize;
    }
    return (qint64) size;
}

/**
 * @brief FileModel::extractWgt unzip the widget in the local_file_info.absolutePath()
 * which can be the cache directory or the local directory on usb, hardisc etc
 * @return
 */
bool Wgt::extract(FreeDiscSpace *MyDiscSpace)
{
    if (!isOpen())
        return false;


    if (!mustExtract())
        return true;

    QFileInfo wgt_file(local_file_path);
    qint64 calc = MyDiscSpace->calculateNeededDiscSpaceToFree(calculateUncompressedSize());
    if (calc > 0 && !MyDiscSpace->freeDiscSpace(calc))
    {
        qCritical(ContentManager) << local_file_path << " Widget was not extracted. Not enough space could be freeed";
        return "";
    }

    // wgt_file.absolutePath() fails when files has an . in name like xyz.secret.wgt
    // we need to get full path - .wgt

    QString folder_path = wgt_file.absoluteFilePath().mid(0, wgt_file.absoluteFilePath().length()-4);
    QDir wgt_dir(folder_path);
    if (wgt_dir.exists() && !wgt_dir.removeRecursively())
    {
        qWarning(ContentManager) << local_file_path << " Widget was not extracted. Previuos directory cannot be deleted";
        return false;
    }
    if (!wgt_dir.mkdir(folder_path))
    {
        qWarning(ContentManager) << local_file_path << " Widget was not extracted. Directory cannot be created";
        return false;
    }

    JlCompress::extractDir(local_file_path, folder_path+"/");

    return true;
}

bool Wgt::mustExtract()
{
    QFileInfo wgt_file(local_file_path);
    QFileInfo wgt_dir(wgt_file.absolutePath()+"/"+wgt_file.baseName());

    QDateTime file = wgt_file.lastModified();
    QDateTime dir  = wgt_dir.lastModified();
    if (wgt_dir.exists() && dir > file)
        return false;
    else
        return true;
}
