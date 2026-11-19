/*
 garlic-player: SMIL Player for Digital Signage
 Copyright (C) 2026 Nikolaos Sagiadinos <garlic@saghiadinos.de>
 This file is part of the GarlicSignage source code

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License, version 3,
 as published by the Free Software Foundation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef GARLIC_CORE_SYSTEM_SYSTEM_INFO_HPP
#define GARLIC_CORE_SYSTEM_SYSTEM_INFO_HPP

#include "reports/system_info.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace garlic::core::system
{
    /// Disk space of the cache, garlic-lib: DiscSpace - 4 % of the disk stay free for the operating system.
    struct DiskSpace
    {
        std::uint64_t total = 0; // 96 % of the disk
        std::uint64_t free  = 0; // available minus the 4 % reserve, at least 0
    };

    /// Time of all cores since the boot: busy and total (busy + idle). The unit differs per platform, only the ratio of
    /// two differences counts.
    struct SystemCpu
    {
        std::uint64_t busy  = 0;
        std::uint64_t total = 0;
    };

    /// Memory and threads of the player itself, for the debug display of the frontends (garlic-lib: ResourceMonitor).
    struct ProcessInfo
    {
        std::uint64_t memory  = 0; // resident set size in bytes
        int           threads = 0;
        /// CPU time (user and system) since the start of the player and of the processes it started, e.g. those of
        /// CEF; the core makes the load in percent out of two of them. Empty where the platform cannot tell.
        std::optional<std::chrono::nanoseconds> cpu_time;
        /// The whole system, for its load beside that of the player. Empty where the platform cannot tell (Android
        /// from version 8 on does not let an app read /proc/stat).
        std::optional<SystemCpu> system_cpu;
    };

    /// Everything for the reports, on the native platforms (garlic-lib: SystemInfos). start_time: ISO 8601.
    reports::SystemInfo collectSystemInfo(std::string_view cache_dir, std::string_view start_time);

    DiskSpace   diskSpace(std::string_view cache_dir);
    ProcessInfo processInfo();

    // ------------------------------------------------------------------ text parsing, testable on every platform

    struct Memory
    {
        std::optional<std::uint64_t> total;
        std::optional<std::uint64_t> available;
    };

    /// /proc/meminfo: MemTotal and MemAvailable in bytes; MemFree if MemAvailable is missing (kernel before 3.14).
    Memory parseMeminfo(std::string_view text);

    /// /proc/self/status: VmRSS in bytes and Threads.
    ProcessInfo parseProcStatus(std::string_view text);

    /// Of /proc/<pid>/stat: the parent and the CPU time in clock ticks, its own (utime, stime) and that of its ended
    /// children (cutime, cstime)
    struct ProcStat
    {
        int           parent = 0;
        std::uint64_t ticks  = 0;
    };

    /// /proc/<pid>/stat; the name in parentheses may hold spaces and parentheses, so the fields count from the last ")"
    std::optional<ProcStat> parseProcStat(std::string_view text);

    /// The first line of /proc/stat ("cpu  user nice system idle iowait irq softirq steal ..."): idle and iowait are
    /// idle, the rest busy; guest is already in user.
    std::optional<SystemCpu> parseStatCpu(std::string_view text);

    /// "de_DE.UTF-8", "de-DE", "de_DE@euro" → "de_DE"; empty or "POSIX" → "C" like QLocale.
    std::string normalizeLocale(std::string_view value);

    /// TZ or the target of /etc/localtime: ":Europe/Berlin", "/usr/share/zoneinfo/Europe/Berlin" → "Europe/Berlin".
    std::string zoneName(std::string_view value);
}

#endif // GARLIC_CORE_SYSTEM_SYSTEM_INFO_HPP
