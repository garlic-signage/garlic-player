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

TEST_CASE("task execution report: the tasks in one taskState")
{
    std::string tasks = "<task id=\"a\"><state>completed</state></task><task id=\"b\"><state>pending</state></task>";
    CHECK(taskExecutionReport("uuid-1", now, tasks) == head + "<taskState>" + tasks + "</taskState></player></report>");
}

TEST_CASE("system info: the CPU load of the system as cpuUsage with %, empty where unknown")
{
    SystemInfo info = system();
    info.cpu_usage  = 12;
    std::string report = inventoryReport("uuid-1", now, info, {});
    CHECK(report.find("<cpuUsage>12%</cpuUsage>") != std::string::npos);
    CHECK(inventoryReport("uuid-1", now, system(), {}).find("<cpuUsage></cpuUsage>") != std::string::npos);
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

TEST_CASE("system report: as the Android players of IAdea, the own values as garlic.* props")
{
    PlayerInfo player;
    player.uuid        = "uuid-1";
    player.app_name    = "garlic-player";
    player.os          = "linux";
    player.version     = "2.0.0";
    player.player_name = "Lobby";
    player.index_uri   = "http://hub/index.smil";
    player.reboot_days = "Sun|Mon";
    player.reboot_time = "04:00";
    player.volume      = "80%";
    SystemInfo info          = system();
    info.memory_used         = 1024;
    info.hdmi_output         = "1920x1080p-60";
    info.device_model        = "Raspberry Pi 4 Model B Rev 1.4";
    info.device_manufacturer = "Raspberry Pi Foundation";
    info.own                 = {{"garlic.uptime.player", "3600"}};
    std::string report = systemReport(now, info, player);

    CHECK(report.starts_with(head + "<systemInfo>"));
    CHECK(report.contains("<memoryTotal>2048</memoryTotal><memoryUsed>1024</memoryUsed><hdmiOutput>1920x1080p-60</hdmiOutput>"
                          "<network><interface id=\"eth0\"><type>ethernet</type><mac>aa:bb</mac><ip>10.0.0.2</ip>"
                          "<netmask>255.0.0.0</netmask><ip_v6>fe80::1</ip_v6><netmask_v6>ffff::</netmask_v6></interface>"
                          "</network>"));
    CHECK(report.contains(R"(<modelInfo><prop name="PCB" value="Raspberry Pi 4 Model B Rev 1.4"/><prop name="PCBRevision" value=""/>)"
                          R"(<prop name="licenseModel" value="linux"/><prop name="manufacturer" value="Sagiadinos"/>)"));
    CHECK(report.contains(R"(<prop name="modelDescription" value="2.0.0"/><prop name="modelName" value="garlic-player"/>)"));
    CHECK(report.contains(R"(<userPref><prop name="info.playerName" value="Lobby"/>)"));
    CHECK(report.contains(R"(<prop name="hardware.videoOut.0.rotation" value="auto"/><prop name="schedule.reboot.days" value="Sun|Mon"/>)"
                          R"(<prop name="schedule.reboot.time" value="04:00"/><prop name="hardware.audioOut.0.masterSoundLevel" value="0.8"/>)"));
    CHECK_FALSE(report.contains("display.brightness")); // not set
    CHECK(report.contains(R"(<prop name="time.timeZone" value="Europe/Berlin"/><prop name="content.serverUrl" value="http://hub/index.smil"/>)"
                          R"(<prop name="garlic.device.manufacturer" value="Raspberry Pi Foundation"/>)"
                          R"(<prop name="garlic.uptime.player" value="3600"/></userPref></configuration></systemInfo>)"));
    CHECK(report.ends_with(R"(<hardwareInfo><hardware id="display:0"><prop name="modelName" value=""/><prop name="productId" value=""/>)"
                           R"(<prop name="serialNumber" value=""/><prop name="vendorId" value=""/></hardware></hardwareInfo></player></report>)"));
}

TEST_CASE("system report: the screen of a supervisor in hardwareInfo (Philips over SICP)")
{
    PlayerInfo player;
    SystemInfo info         = system();
    info.display_model      = "43BDL4550D/00";
    info.display_serial     = "AU0A2123004336";
    info.display_vendor     = "Philips";
    CHECK(systemReport(now, info, player)
              .ends_with(R"(<hardwareInfo><hardware id="display:0"><prop name="modelName" value="43BDL4550D/00"/>)"
                         R"(<prop name="productId" value=""/><prop name="serialNumber" value="AU0A2123004336"/>)"
                         R"(<prop name="vendorId" value="Philips"/></hardware></hardwareInfo></player></report>)"));
}
