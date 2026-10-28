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
#include "resource_monitor.h"

ResourceMonitor::ResourceMonitor(QObject *parent) : QObject(parent)
{

}

void ResourceMonitor::refresh()
{
    determineDiscSpace();
    determineMemorySystem();
    determineMemoryApp();
    determineThreads();
}

QString ResourceMonitor::getTotalDiscSpace() const
{
    return total_disc_space;
}

QString ResourceMonitor::getFreeDiscSpace() const
{
    return free_disc_space;
}

QString ResourceMonitor::getTotalMemorySystem() const
{
    return total_memory_system;
}

QString ResourceMonitor::getFreeMemorySystem() const
{
    return free_memory_system;
}

QString ResourceMonitor::getMemoryAppUse() const
{
    return memory_apps_use;
}

QString ResourceMonitor::getMaxMemoryAppUsed() const
{
    return max_memory_app_used;
}

QString ResourceMonitor::getThreadsNumber() const
{
    return threads_number;
}

QString ResourceMonitor::getMaxThreadsNumber() const
{
    return max_threads_number;
}

void ResourceMonitor::setDiscSpace(SystemInfos::DiscSpace *ds)
{
    MyDiscSpace = ds;
}

void ResourceMonitor::determineDiscSpace()
{
    MyDiscSpace->recalculate();

    total_disc_space = "Total Disc: " + determineNumberUnit(MyDiscSpace->getBytesTotal());
    free_disc_space = "Free Disc: " + determineNumberUnit(MyDiscSpace->getBytesFree() + MyDiscSpace->getBytesLocked());
}

void ResourceMonitor::determineMemorySystem()
{
    MyMemoryInfos.refresh();
    total_memory_system = "Total Memory System: " + determineNumberUnit(MyMemoryInfos.getTotal());
    free_memory_system  = "Free Memory System: " + determineNumberUnit(MyMemoryInfos.getFree());
}

void ResourceMonitor::determineMemoryApp()
{
    qint64  current_rss = MyMemoryInfos.getRSS();
    if (current_rss > max_memory_used)
    {
        max_memory_used = current_rss;
        max_memory_time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:s");
    }
    memory_apps_use     = "App Memory use: " + determineNumberUnit(current_rss);
    max_memory_app_used = "Max Memory App used: "  + determineNumberUnit(max_memory_used) + " at " + max_memory_time;
}

void ResourceMonitor::determineThreads()
{
    qint64  current_threads = MyGeneralInfos.countThreads();
    if (current_threads > max_threads_used)
        max_threads_used = current_threads;
    threads_number     = "Threads: " + QString::number(current_threads);
    max_threads_number = "Max Threads: " + QString::number(max_threads_used);
}

QString ResourceMonitor::determineNumberUnit(qint64 number)
{
    QLocale locale(QLocale::English);
    return locale.formattedDataSize(number);
}
