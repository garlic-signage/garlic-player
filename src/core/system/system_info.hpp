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

    /// Memory and threads of the player itself, for the debug display of the frontends (garlic-lib: ResourceMonitor).
    struct ProcessInfo
    {
        std::uint64_t memory  = 0; // resident set size in bytes
        int           threads = 0;
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

    /// "de_DE.UTF-8", "de-DE", "de_DE@euro" → "de_DE"; empty or "POSIX" → "C" like QLocale.
    std::string normalizeLocale(std::string_view value);

    /// TZ or the target of /etc/localtime: ":Europe/Berlin", "/usr/share/zoneinfo/Europe/Berlin" → "Europe/Berlin".
    std::string zoneName(std::string_view value);
}

#endif // GARLIC_CORE_SYSTEM_SYSTEM_INFO_HPP
