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
#ifndef BASE_MANAGER_H
#define BASE_MANAGER_H

#include <QFile>
#include "download_queue.h"
#include "tools/main_configuration.hpp"
#include "files/free_disc_space.h"

namespace Files
{
    class BaseManager : public QObject
    {
            Q_OBJECT
        public:
            BaseManager(MainConfiguration *config, FreeDiscSpace *fds, QObject *parent = Q_NULLPTR);

        protected:
            MainConfiguration *MyConfiguration;
            FreeDiscSpace     *MyFreeDiscSpace;
            bool               isRemote(QString src);
            bool               isRelative(QString src);
            void               renameDownloadedFile(QString file_path);
    };
}
#endif // BASE_MANAGER_H
