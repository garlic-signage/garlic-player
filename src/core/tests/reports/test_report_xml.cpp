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

#include "reports/report_xml.hpp"

using namespace garlic::core::reports;
using namespace std::chrono;

namespace
{
    const auto now = sys_days{October / 28 / 2026} + 7h + 30min;

    const std::string head = R"(<?xml version="1.0" encoding="UTF-8"?><report xmlns="http://schemas.garlic-player.com/gapi-1.0">)"
                             "<date>2026-10-28T07:30:00Z</date><version>1.0</version><player id=\"uuid-1\">";

    SystemInfo system()
    {
        SystemInfo s;
        s.start_time     = "2026-10-28T06:00:00";
        s.time_zone      = "Europe/Berlin";
        s.total_capacity = 1000;
        s.free_space     = 400;
        s.memory_total   = 2048;
        s.locale         = "de_DE";
        s.network        = {{"eth0", "aa:bb", "ethernet", {{NetworkAddress::Family::V4, "10.0.0.2", "255.0.0.0", "10.255.255.255"},
                                                     {NetworkAddress::Family::V6, "fe80::1", "ffff::"}}}};
        return s;
    }
}

TEST_CASE("play and event log: the lines of the log in the report")
{
    std::string lines = "<contentPlayed><contentId>a&amp;b</contentId></contentPlayed>\n";
    CHECK(playLogReport("uuid-1", now, lines) == head + "<contentPlayLog>" + lines + "</contentPlayLog></player></report>");
    CHECK(eventLogReport("uuid-1", now, "<event/>") == head + "<playerEventLog><event/></playerEventLog></player></report>");
}

TEST_CASE("inventory report: system info and one object per file, values escaped")
{
    std::vector<InventoryObject> objects{{"abc", "http://hub/a.jpg?x=1&y=2", "COMPLETE", "image/jpeg", 100,
                                          "2026-10-28 07:00:00", 100}};
    CHECK(inventoryReport("uuid-1", now, system(), objects) ==
          head +
              "<systemInfo><systemStartTime>2026-10-28T06:00:00</systemStartTime><systemTimeZone>Europe/Berlin"
              "</systemTimeZone><totalCapacity>1000</totalCapacity><totalFreeSpace>400</totalFreeSpace><cpuUsage>"
              "</cpuUsage><memoryTotal>2048</memoryTotal><memoryUsed></memoryUsed><hdmiOutput></hdmiOutput></systemInfo>"
              "<contentDirectory><objectInfo><object id=\"abc\" persistent=\"false\"><resourceURI>"
              "http://hub/a.jpg?x=1&amp;y=2</resourceURI><state>COMPLETE</state><contentType>image/jpeg</contentType>"
              "<contentLength>100</contentLength><lastModified>2026-10-28 07:00:00</lastModified><expires></expires>"
              "<transferLength>100</transferLength></object></objectInfo></contentDirectory></player></report>");
}

TEST_CASE("system report: network, configuration and hardware like garlic-lib")
{
    PlayerInfo player{"uuid-1", "garlic-player", "linux", "Lobby", "http://hub/index.smil"};
    std::string report = systemReport(now, system(), player);

    CHECK(report.starts_with(head + "<systemInfo><systemStartTime>"));
    CHECK(report.find("<hdmiOutput></hdmiOutput><network><interface id=\"eth0\"><mac>aa:bb</mac><type>ethernet</type>"
                      "<ip_v4>10.0.0.2</ip_v4><netmask_v4>255.0.0.0</netmask_v4><broadcast_v4>10.255.255.255"
                      "</broadcast_v4><ip_v6>fe80::1</ip_v6><netmask_v6>ffff::</netmask_v6></interface></network>"
                      "<configuration><modelInfo><prop name=\"PCB\" value=\"garlic-player\"/>") != std::string::npos);
    CHECK(report.find("<prop name=\"operatingSystem\" value=\"linux\"/><prop name=\"manufacturer\" value=\"Sagiadinos\"/>") !=
          std::string::npos);
    CHECK(report.find("<userPref><prop name=\"info.playerName\" value=\"Lobby\"/>") != std::string::npos);
    CHECK(report.find("<prop name=\"content.serverUrl\" value=\"http://hub/index.smil\"/></userPref></configuration>"
                      "</systemInfo><hardwareInfo><hardware id=\"display:0\">") != std::string::npos);
    CHECK(report.ends_with("<prop name=\"vendorId\" value=\"\"/></hardware></hardwareInfo></player></report>"));
}
