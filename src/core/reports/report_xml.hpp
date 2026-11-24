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
#ifndef GARLIC_CORE_REPORTS_REPORT_XML_HPP
#define GARLIC_CORE_REPORTS_REPORT_XML_HPP

#include "reports/system_info.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace garlic::core::reports
{
    /// One file in the cache for the inventory report.
    struct InventoryObject
    {
        std::string   id;           // file name without extension
        std::string   resource_uri; // URL
        std::string   state;        // TRANSFER, COMPLETE
        std::string   content_type;
        std::uint64_t content_length = 0;
        std::string   last_modified;
        std::uint64_t transfer_length = 0; // bytes in the cache so far
    };

    /**
     * The reports in the format of garlic-lib (reports/create), so the hub reads them as before:
     *   <?xml version="1.0" encoding="UTF-8"?>
     *   <report xmlns="http://schemas.garlic-player.com/gapi-1.0"><date>UTC</date><version>1.0</version>
     *     <player id="uuid"> … </player></report>
     * Values are escaped. The lines of a log are taken as they are: the logger writes them as escaped XML.
     */
    std::string playLogReport(std::string_view uuid, std::chrono::system_clock::time_point now, std::string_view lines);
    std::string eventLogReport(std::string_view uuid, std::chrono::system_clock::time_point now, std::string_view lines);
    /// tasks: <task id="…"> elements of the task log, one per task; garlic-lib had no such report.
    std::string taskExecutionReport(std::string_view uuid, std::chrono::system_clock::time_point now,
                                    std::string_view tasks);
    std::string inventoryReport(std::string_view uuid, std::chrono::system_clock::time_point now,
                                const std::vector<InventoryObject> &objects);
    /**
     * modelInfo of the system report and of /v2/system/modelInfo, without option: the hardware the player runs on.
     * Model of the supervisor, else that of the platform; manufacturer of the platform; firmware of the supervisor as
     * PCBRevision. Without a known model or manufacturer the player stands in: garlic-player, its version, Sagiadinos.
     */
    std::vector<std::pair<std::string, std::string>> modelInfo(const SystemInfo &system, const PlayerInfo &player);
    /// As the Android players of IAdea, with our namespace; see systemInfo and configuration in report_xml.cpp, the own
    /// values as props "garlic.*".
    std::string systemReport(std::chrono::system_clock::time_point now, const SystemInfo &system,
                             const PlayerInfo &player);
}

#endif // GARLIC_CORE_REPORTS_REPORT_XML_HPP
