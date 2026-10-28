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
#ifndef MEDIA_MODEL_H
#define MEDIA_MODEL_H

#include <QObject>
#include <QMap>
#include <QCryptographicHash>
#include "tools/main_configuration.hpp"
#include "files/wgt.h"


const     int        MEDIA_NOT_EXISTS      = 0;
const     int        MEDIA_CACHED          = 1;
const     int        MEDIA_MODIFIED        = 2;
const     int        MEDIA_UNCACHABLE      = 3;
const     int        MEDIA_IS_LOCAL        = 4;
const     int        MEDIA_DOWNLOAD_FAILED = 5;

/**
 * @brief The FileModel class handles the file management list
 *
 * - handles different media files eg extracting of wgt
 * - organize a Map of available Files with their "real" path and statuses
 */
class MediaModel : public QObject
{
    Q_OBJECT
    public:

        explicit MediaModel(FreeDiscSpace *fds, QObject *parent=Q_NULLPTR);
        ~MediaModel();

        void                                          clearQueue();
        QString                                       findLocalBySrcPath(QString src_file_path);
        int                                           findStatusBySrcPath(QString src_file_path);
        void                                          setStatusBySrcPath(QString src_file_path, int status);
        void                                          insertAsUncachable(QString src_file_path);
        void                                          insertAsLocalFile(QString src_file_path);
        void                                          insertCacheableFile(QString src_file_path, QString local_file_path);
        QString                                       handleWgt(QString wgt_file_path);
        //Getter/Setter
        QMap<QString, QPair<QString, int> >           getAvailableMediaList() const {return available_media_list;}
        int                                           countAvailableMedia(){return available_media_list.size();}
        void                                          setAvailableMediaList(const QMap<QString, QPair<QString, int> > &value) {available_media_list = value;}
        QString                                       determineHashedFilePath(QString src_file_path);
        FreeDiscSpace                                *MyFreeDiscSpace;

    protected:
        QMap<QString, QPair<QString, int>>            available_media_list;
        QMap<QString, QPair<QString, int>>::iterator  i_available_media_list;
        QString                                       determinePathByMedia(QString src_file_path, QString local_file_path);

};

#endif // MEDIA_MODEL_H
