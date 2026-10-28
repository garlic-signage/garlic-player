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
#include "memory.h"

SystemInfos::Memory::Memory(QObject *parent) : QObject(parent)
{

}

void SystemInfos::Memory::refresh()
{
    detectGlobal();
    detectProcess();
}

qint64 SystemInfos::Memory::getRSS()
{
    return rss;
}

void SystemInfos::Memory::setRSS(qint64 value)
{
    rss = value;
}

qint64 SystemInfos::Memory::getUsed()
{
    return used;
}

void SystemInfos::Memory::setUsed(qint64 value)
{
    used = value;
}

qint64 SystemInfos::Memory::getFree()
{
    return free;
}

void SystemInfos::Memory::setFree(qint64 value)
{
    free = value;
}

qint64 SystemInfos::Memory::getTotal()
{
    return total;
}

void SystemInfos::Memory::setTotal(qint64 value)
{
    total = value;
}

// ================ protected methods ======================

void SystemInfos::Memory::detectGlobal()
{
    setFree(0);
    setTotal(0);
    setUsed(0);

#if defined Q_OS_WIN32
    MEMORYSTATUSEX memory_status;

    memory_status.dwLength = sizeof (memory_status);
    GlobalMemoryStatusEx (&memory_status);

    setFree((qint64)memory_status.ullAvailPhys);
    setTotal((qint64)memory_status.ullTotalPhys);
#elif defined Q_OS_DARWIN
    int mib[2];
    int64_t physical_memory;
    size_t length;

    // Get the Physical memory size
    mib[0] = CTL_HW;
    mib[1] = HW_MEMSIZE;
    length = sizeof(int64_t);
    sysctl(mib, 2, &physical_memory, &length, NULL, 0);

    setTotal((qint64)physical_memory);

    mach_port_t host_port;
    mach_msg_type_number_t host_size;
    vm_size_t pagesize;
    host_port = mach_host_self();
    host_size = sizeof(vm_statistics_data_t) / sizeof(integer_t);
    host_page_size(host_port, &pagesize);
    vm_statistics_data_t vm_stat;
    host_statistics(host_port, HOST_VM_INFO, (host_info_t)&vm_stat, &host_size);
    /* Stats in bytes */
    natural_t mem_free = vm_stat.free_count * pagesize;
    setFree((qint64)mem_free);

#elif defined Q_OS_LINUX // work on Android too
    QFile fp("/proc/meminfo");
    if (!fp.open(QIODevice::ReadOnly))
        return;
    QTextStream in(&fp);
    QRegularExpression rx("[0-9]+");                            // match only the numbers from a string
    QRegularExpressionMatch match = rx.match(in.readLine(500)); // first line ist MemTotal
    setTotal(match.captured(0).toLong() * 1024);                // * 1024 to get Bytes even if return kB

    match = rx.match(in.readLine(500));                         // second line ist MemFree
    setFree(match.captured(0).toLong() * 1024);                 // * 1024 to get Bytes even if return kB

    fp.close();
#endif

    setUsed(getTotal() - getFree());
}


void SystemInfos::Memory::detectProcess()
{
    setRSS(0);
#if defined Q_OS_WIN32
    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo( GetCurrentProcess( ), &info, sizeof(info) );
    setRSS((qint64)info.WorkingSetSize);

#elif defined Q_OS_DARWIN
    struct mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if ( task_info( mach_task_self( ), MACH_TASK_BASIC_INFO,
        (task_info_t)&info, &infoCount ) != KERN_SUCCESS )
        return;
    setRSS((qint64)info.resident_size);

#elif defined Q_OS_LINUX // work on Android too
    QFile fp("/proc/self/statm");
    if (!fp.open(QIODevice::ReadOnly))
        return;
    QTextStream in(&fp);
    QStringList fields = in.readLine(500).split(" ");
    fp.close();
    setRSS(fields.at(1).toLong() * (qint64)sysconf(_SC_PAGESIZE));
#endif
}

