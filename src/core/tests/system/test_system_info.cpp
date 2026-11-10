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
#include "time_zone.hpp"

#if defined(__linux__) || defined(__APPLE__)
    #include "system/ifaddrs_network.hpp"
#endif

#include <algorithm>
#include <filesystem>

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

TEST_CASE("/proc fields: the key exactly, the number of its own line, too large is nothing")
{
    Memory memory = parseMeminfo("MemTotalX:  5 kB\nMemTotal:  1000 kB\nMemAvailable: kB\nMemFree:  400 kB\n");
    CHECK(memory.total == 1000ULL * 1024);      // not MemTotalX
    CHECK(memory.available == 400ULL * 1024);   // MemAvailable without a number: MemFree
    CHECK_FALSE(parseMeminfo("MemTotal kB 5\n").total.has_value());
    CHECK_FALSE(parseMeminfo("MemTotal: kB\nMemFree: 400 kB\n").total.has_value()); // not the number of the next line
    CHECK_FALSE(parseMeminfo("MemTotal: 99999999999999999999999 kB\n").total.has_value());
}

TEST_CASE("locale: C with an encoding is C, a name without : stays as it is")
{
    CHECK(normalizeLocale("C.UTF-8") == "C"); // Debian containers
    CHECK(normalizeLocale("C") == "C");
    CHECK(zoneName("Europe/Berlin") == "Europe/Berlin");
}

TEST_CASE("disk space: 96 % of the disk, free is available minus the 4 % reserve")
{
    std::error_code ec;
    auto            before = std::filesystem::space(".", ec);
    REQUIRE_FALSE(ec);
    DiskSpace disk  = diskSpace(".");
    auto      after = std::filesystem::space(".", ec);

    std::uint64_t reserved = before.capacity - before.capacity / 100 * 96;
    CHECK(disk.total == before.capacity / 100 * 96);
    CHECK(disk.free + reserved >= std::min(before.available, after.available)); // the disk may change meanwhile
    CHECK(disk.free + reserved <= std::max(before.available, after.available));
}

TEST_CASE("system info: no disk values for a missing cache directory, memory used is more than 0")
{
    auto info = collectSystemInfo("/this/does/not/exist", "");
    CHECK_FALSE(info.total_capacity.has_value());
    CHECK_FALSE(info.free_space.has_value());
    REQUIRE(info.memory_used.has_value());
    CHECK(*info.memory_used > 0);
}

#if defined(__linux__) && !defined(__ANDROID__)
TEST_CASE("Linux: the time zone from TZ if it is a name, otherwise from the system")
{
    std::string system_zone;
    {
        garlic::core::tests::TimeZone tz("CET-1"); // POSIX rule, not a name
        system_zone = collectSystemInfo(".", "").time_zone;
        CHECK(system_zone != "CET-1");
    }
    garlic::core::tests::TimeZone tz(":America/New_York");
    CHECK(collectSystemInfo(".", "").time_zone == "America/New_York");
}
#endif

#if defined(__linux__) || defined(__APPLE__)
TEST_CASE("MAC in upper case with colons like Qt")
{
    const unsigned char bytes[] = {0x00, 0x1a, 0x2b, 0xff};
    CHECK(garlic::core::system::platform::macText(bytes, 4) == "00:1A:2B:FF");
    CHECK(garlic::core::system::platform::macText(bytes, 0).empty());
}
#endif

#ifdef __linux__
TEST_CASE("Linux: the loopback interface with type, MAC and its addresses")
{
    auto info = collectSystemInfo(".", "");
    auto lo   = std::ranges::find(info.network, std::string("lo"), &garlic::core::reports::NetworkInterface::id);
    REQUIRE(lo != info.network.end());
    CHECK(lo->type == "loopback");
    CHECK(lo->mac == "00:00:00:00:00:00");

    using Family = garlic::core::reports::NetworkAddress::Family;
    auto v4 = std::ranges::find(lo->addresses, std::string("127.0.0.1"), &garlic::core::reports::NetworkAddress::ip);
    REQUIRE(v4 != lo->addresses.end());
    CHECK(v4->family == Family::V4);
    CHECK(v4->netmask == "255.0.0.0");
    CHECK(v4->broadcast.empty()); // loopback has no broadcast

    for (const auto &interface : info.network) // the family fits the address
        for (const auto &address : interface.addresses)
        {
            CAPTURE(address.ip);
            CHECK((address.family == Family::V6) == address.ip.contains(':'));
        }

    auto v6 = std::ranges::find(lo->addresses, std::string("::1"), &garlic::core::reports::NetworkAddress::ip);
    if (v6 != lo->addresses.end()) // IPv6 may be switched off, e.g. in containers
    {
        CHECK(v6->family == Family::V6);
        CHECK(v6->netmask == "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
    }
}
#endif
