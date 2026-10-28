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
#include <doctest/doctest.h>

#include "system/system_info.hpp"

using namespace garlic::core::system;

TEST_CASE("meminfo: MemTotal and MemAvailable in bytes")
{
    Memory memory = parseMeminfo("MemTotal:       16314508 kB\nMemFree:          812344 kB\n"
                                 "MemAvailable:    9123456 kB\nBuffers:          123 kB\n");
    CHECK(memory.total == 16314508ULL * 1024);
    CHECK(memory.available == 9123456ULL * 1024); // not MemFree: the file cache can be freed

    Memory old_kernel = parseMeminfo("MemTotal: 1000 kB\nMemFree: 400 kB\n");
    CHECK(old_kernel.available == 400ULL * 1024);

    CHECK_FALSE(parseMeminfo("").total.has_value());
}

TEST_CASE("process status: VmRSS and Threads")
{
    ProcessInfo info = parseProcStatus("Name:\tgarlic\nVmRSS:\t   51200 kB\nThreads:\t7\n");
    CHECK(info.memory == 51200ULL * 1024);
    CHECK(info.threads == 7);
}

TEST_CASE("locale like QLocale::name()")
{
    CHECK(normalizeLocale("de_DE.UTF-8") == "de_DE");
    CHECK(normalizeLocale("de-DE") == "de_DE");
    CHECK(normalizeLocale("de_DE@euro") == "de_DE");
    CHECK(normalizeLocale("POSIX") == "C");
    CHECK(normalizeLocale("") == "C");
}

TEST_CASE("zone name from TZ or the target of /etc/localtime")
{
    CHECK(zoneName(":Europe/Berlin") == "Europe/Berlin");
    CHECK(zoneName("/usr/share/zoneinfo/America/New_York") == "America/New_York");
    CHECK(zoneName("../usr/share/zoneinfo/UTC") == "UTC");
}

TEST_CASE("disk space keeps 4 % for the system, a missing directory gives nothing")
{
    DiskSpace disk = diskSpace(".");
    CHECK(disk.total > 0);
    CHECK(disk.free <= disk.total);

    DiskSpace missing = diskSpace("/this/does/not/exist");
    CHECK(missing.total == 0);
}

TEST_CASE("the real values of this machine")
{
    auto info = collectSystemInfo(".", "2026-10-28T08:00:00+01:00");
    CHECK(info.start_time == "2026-10-28T08:00:00+01:00");
    REQUIRE(info.memory_total.has_value());
    CHECK(*info.memory_total > 0);
    REQUIRE(info.memory_used.has_value());
    CHECK(*info.memory_used <= *info.memory_total);
    CHECK(info.total_capacity.has_value());
    CHECK_FALSE(info.locale.empty());
    for (const auto &interface : info.network)
    {
        CAPTURE(interface.id);
        CHECK_FALSE(interface.mac.empty());
        CHECK_FALSE(interface.addresses.empty());
    }

    ProcessInfo self = processInfo();
    CHECK(self.memory > 0);
    CHECK(self.threads >= 1);
}
