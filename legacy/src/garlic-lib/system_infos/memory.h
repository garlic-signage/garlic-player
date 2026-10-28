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
#ifndef MEMORY_INFOS_H
#define MEMORY_INFOS_H

#include <QObject>
#include <QRegularExpression>
#include <QFile>
#include <QTextStream>



#if defined Q_OS_WIN32
    #define NOMINMAX   // must defined before windows.h else AVPlayer.h runs in errors about ::max
    #include <windows.h>
    #include <psapi.h>
#elif defined Q_OS_DARWIN
    #include <mach/mach.h>
    #include <mach/mach_host.h>
    #include <sys/types.h>
    #include <sys/sysctl.h>
#elif defined Q_OS_LINUX
    #include <unistd.h>
#endif

namespace SystemInfos
{
    class Memory: public QObject
    {
            Q_OBJECT
        public:
            explicit Memory(QObject *parent = nullptr);
            void     refresh();
            qint64   getRSS();
            void   setRSS(qint64 value);
            qint64   getUsed();
            void setUsed(qint64 value);
            qint64   getFree();
            void   setFree(qint64 value);
            qint64   getTotal();
            void setTotal(qint64 value);
        protected:
            qint64 rss, used, free, total = 0;
            void detectGlobal();
            void detectProcess();

    };
}
#endif // MEMORY_INFOS_H
