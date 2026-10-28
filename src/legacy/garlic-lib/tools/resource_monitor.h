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
#ifndef RESOURCE_MONITOR_H
#define RESOURCE_MONITOR_H

#include <QObject>
#include <QDateTime>
#include "system_infos/memory.h"
#include "system_infos/general.h"
#include "system_infos/disc_space.hpp"

class ResourceMonitor : public QObject
{
        Q_OBJECT
    public:
        explicit ResourceMonitor(QObject *parent = nullptr);
        Q_INVOKABLE void refresh();
        Q_INVOKABLE QString getTotalDiscSpace() const;
        Q_INVOKABLE QString getFreeDiscSpace() const;
        Q_INVOKABLE QString getTotalMemorySystem() const;
        Q_INVOKABLE QString getFreeMemorySystem() const;
        Q_INVOKABLE QString getMemoryAppUse() const;
        Q_INVOKABLE QString getMaxMemoryAppUsed() const;
        Q_INVOKABLE QString getThreadsNumber() const;
        Q_INVOKABLE QString getMaxThreadsNumber() const;

        void setDiscSpace(SystemInfos::DiscSpace *ds);
protected:
        QString              total_disc_space = "";
        QString              free_disc_space = "";
        QString              total_memory_system = "";
        QString              free_memory_system = "";
        QString              memory_apps_use = "";
        QString              max_memory_app_used = "";
        QString              threads_number = "";
        QString              max_threads_number = "";
        qint64               max_memory_used = 0;
        QString              max_memory_time = "";
        qint64               max_threads_used = 0;
        SystemInfos::DiscSpace *MyDiscSpace;
        SystemInfos::Memory     MyMemoryInfos;
        SystemInfos::General    MyGeneralInfos;
        void determineDiscSpace();
        void determineMemorySystem();
        void determineMemoryApp();
        void determineThreads();
        QString determineNumberUnit(qint64 number);

};

#endif // RESOURCE_MONITOR_H
