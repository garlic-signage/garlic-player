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
#ifndef GENERAL_H
#define GENERAL_H

#include <QObject>
#include <QFile>
#include <QTextStream>

#if defined Q_OS_WIN32
    #define NOMINMAX   // must defined before windows.h else AVPlayer.h runs in errors about ::max
    #include <windows.h>
    #include <tlhelp32.h>
#endif

namespace SystemInfos
{
    class General : public QObject
    {
            Q_OBJECT
        public:
            explicit General(QObject *parent = nullptr);

            int countThreads();
        signals:

        public slots:
    };
}
#endif // GENERAL_H
