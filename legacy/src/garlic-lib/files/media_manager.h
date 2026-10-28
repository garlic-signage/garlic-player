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
#ifndef MEDIA_MANAGER_H
#define MEDIA_MANAGER_H

#include <QSet>
#include "base_manager.h"
#include "media_model.h"

/**
 * @brief The MediaManager class is the "interface" for handling playlist media
 *        It interacts between the parser an
 *
 * - get from parser a file request and delegates it to DownloadQueue not available
 * - transfer media files to MediaModel
 * - answers to a parser request with a playable filepath
 */

namespace Files
{
    class MediaManager : public BaseManager
    {
        Q_OBJECT
    public:
        explicit MediaManager(MediaModel *mm, DownloadQueue *dq, MainConfiguration *config, FreeDiscSpace *fds, QObject *parent=Q_NULLPTR);
        ~MediaManager();
        void                  clearQueues();
        void                  registerFile(QString src);
        void                  checkForUpdate(QString src);
        void                  registerAsUncachable(QString src);
        QString               requestLoadablePath(QString src);
        int                   checkCacheStatus(QString src);
        void                  insertCurrentlyPlaying(QString path);
        bool                  removeCurrentlyPlaying(QString path);
        bool                  isCurrentlyPlaying(QString path);

    protected:
        QSet<QString>         currently_playing;
        int                   timer_id;
        DownloadQueue        *MyDownloadQueue;
        FreeDiscSpace        *MyFreeDiscSpace;
        MediaModel           *MyMediaModel;

        void                  handleUpdated(QString local_path, QString src);
        void                  handleRemoteFile(QString src);
    protected slots:
        void                  doSucceed(QString src_file_path, QString local_file_path);
        void                  doNotCacheable(QString src_file_path);
    };
}
#endif // MEDIA_MANAGER_H
