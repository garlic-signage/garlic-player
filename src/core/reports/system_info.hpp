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
#ifndef GARLIC_CORE_REPORTS_SYSTEM_INFO_HPP
#define GARLIC_CORE_REPORTS_SYSTEM_INFO_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace garlic::core::reports
{
    struct NetworkAddress
    {
        enum class Family
        {
            V4,
            V6
        };
        Family      family = Family::V4;
        std::string ip;
        std::string netmask;
        std::string broadcast{}; // V4 only
    };

    struct NetworkInterface
    {
        std::string                 id; // e.g. "eth0"
        std::string                 mac;
        std::string                 type; // e.g. "ethernet", "wifi"
        std::vector<NetworkAddress> addresses;
    };

    /**
     * What the system report tells about the device, garlic-lib: SystemInfos (DiscSpace, Memory, Network).
     * Depends on the platform, so it is filled by the host; unknown values stay empty.
     */
    struct SystemInfo
    {
        std::string                   start_time; // ISO 8601, when the player started
        std::string                   time_zone;  // e.g. "Europe/Berlin"
        std::optional<std::uint64_t>  total_capacity; // bytes of the cache file system
        std::optional<std::uint64_t>  free_space;
        std::optional<std::uint64_t>  memory_total;
        std::optional<std::uint64_t>  memory_used;
        std::optional<int>            cpu_usage; // percent of all cores, the whole system (garlic-lib: always empty)
        std::optional<int>            cpu_temperature; // °C, as the information dialog (not in IAdea reports)
        std::vector<NetworkInterface> network;
        std::string                   locale; // e.g. "de_DE"
        std::string                   hdmi_output;         // the screen as the frontend tells it: "1920x1080p-60"
        std::string                   device_model;        // the hardware: "Raspberry Pi 4 Model B Rev 1.4", "BDL4550D"
        std::string                   device_manufacturer; // "Raspberry Pi Foundation", "Philips"

        /// The screen as a supervisor knows it (Philips over SICP), hardwareInfo of the system report; empty: unknown
        std::string display_model;
        std::string display_product_id;
        std::string display_serial;
        std::string display_vendor;

        /// Own values of the player (uptime, temperatures …), props "garlic.*" in the userPref of the system report
        std::vector<std::pair<std::string, std::string>> own;
    };

    /// The player itself in the system report.
    struct PlayerInfo
    {
        std::string uuid;
        std::string app_name; // modelName, e.g. "garlic-player"
        std::string os;       // licenseModel: "linux", "android" …
        std::string version;  // modelDescription
        std::string player_name;
        std::string index_uri;
        // of the configuration (ini), empty if not set
        std::string reboot_days;
        std::string reboot_time;
        std::string rotation;
        std::string volume;     // "80" or "80%": masterSoundLevel 0.8
        std::string brightness;
    };
}

#endif // GARLIC_CORE_REPORTS_SYSTEM_INFO_HPP
