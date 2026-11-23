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
#include "reports/report_xml.hpp"
#include "common/time_format.hpp"

#include <algorithm>
#include <charconv>
#include <format>

namespace garlic::core::reports
{
    namespace
    {
        std::string escape(std::string_view text)
        {
            std::string out;
            for (char c : text)
            {
                switch (c)
                {
                    case '&': out += "&amp;"; break;
                    case '<': out += "&lt;"; break;
                    case '>': out += "&gt;"; break;
                    case '"': out += "&quot;"; break;
                    default: out.push_back(c);
                }
            }
            return out;
        }

        std::string tag(std::string_view name, std::string_view value)
        {
            return "<" + std::string(name) + ">" + escape(value) + "</" + std::string(name) + ">";
        }

        std::string number(const std::optional<std::uint64_t> &value)
        {
            return value ? std::to_string(*value) : std::string();
        }

        std::string prop(std::string_view name, std::string_view value)
        {
            return "<prop name=\"" + escape(name) + "\" value=\"" + escape(value) + "\"/>";
        }

        /// garlic-lib: CreateBase::init - the date is UTC like Qt::ISODate.
        std::string begin(std::string_view uuid, std::chrono::system_clock::time_point now)
        {
            auto utc = std::chrono::floor<std::chrono::seconds>(now);
            return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                   "<report xmlns=\"http://schemas.garlic-player.com/gapi-1.0\">" +
                   tag("date", common::isoDateTime(utc) + "Z") + tag("version", "1.0") + "<player id=\"" +
                   escape(uuid) + "\">";
        }

        std::string end()
        {
            return "</player></report>";
        }

        /**
         * As the Android players of IAdea (garlic-lib: CreateBase::createSystemInfo): the CPU load with "%", the screen
         * as "1920x1080p-60". Disk and memory in bytes like garlic-lib; IAdea seems to give the memory in KB, an
         * evaluation has to treat its reports apart. inner is placed before </systemInfo> (network, configuration).
         */
        std::string systemInfo(const SystemInfo &system, std::string_view inner = {})
        {
            return "<systemInfo>" + tag("systemStartTime", system.start_time) + tag("systemTimeZone", system.time_zone) +
                   tag("totalCapacity", number(system.total_capacity)) + tag("totalFreeSpace", number(system.free_space)) +
                   tag("cpuUsage", system.cpu_usage ? std::to_string(*system.cpu_usage) + "%" : "") +
                   tag("memoryTotal", number(system.memory_total)) + tag("memoryUsed", number(system.memory_used)) +
                   tag("hdmiOutput", system.hdmi_output) +
                   std::string(inner) + "</systemInfo>";
        }

        /// As IAdea: type, mac, ip and netmask of the first IPv4 address; then the IPv6 addresses (ip_v6, netmask_v6)
        std::string network(const SystemInfo &system)
        {
            std::string text = "<network>";
            for (const NetworkInterface &interface : system.network)
            {
                text += "<interface id=\"" + escape(interface.id) + "\">" + tag("type", interface.type) +
                        tag("mac", interface.mac);
                bool v4 = false;
                for (const NetworkAddress &address : interface.addresses)
                    if (address.family == NetworkAddress::Family::V4 && !v4)
                    {
                        text += tag("ip", address.ip) + tag("netmask", address.netmask);
                        v4 = true;
                    }
                for (const NetworkAddress &address : interface.addresses)
                    if (address.family != NetworkAddress::Family::V4)
                        text += tag("ip_v6", address.ip) + tag("netmask_v6", address.netmask);
                text += "</interface>";
            }
            return text + "</network>";
        }

        /// "80", "80%" → "0.8" as hardware.audioOut.0.masterSoundLevel of IAdea; else empty
        std::string soundLevel(std::string_view percent)
        {
            if (percent.ends_with('%'))
                percent.remove_suffix(1);
            int value = 0;
            auto [end, error] = std::from_chars(percent.data(), percent.data() + percent.size(), value);
            if (percent.empty() || error != std::errc() || end != percent.data() + percent.size())
                return {};
            return std::format("{}", std::clamp(value, 0, 100) / 100.0);
        }

        /**
         * As the Android players of IAdea: modelInfo (the player is the model, PCB the hardware it runs on), userPref
         * with what is known (the configuration, the system), then the own values as "garlic.*". What the player does
         * not know (proxy, wifi, adb …) is left out.
         */
        std::string configuration(const SystemInfo &system, const PlayerInfo &player)
        {
            std::string text = "<configuration><modelInfo>" + prop("PCB", system.device_model) + prop("PCBRevision", "") +
                               prop("licenseModel", player.os) + prop("manufacturer", "Sagiadinos") +
                               prop("manufacturerURL", "https://garlic-player.com") +
                               prop("modelDescription", player.version) + prop("modelName", player.app_name) +
                               prop("modelURL", "") + prop("option", "") + "</modelInfo>" + "<factoryDefault>" +
                               tag("variant", "") + "</factoryDefault><userPref>" +
                               prop("info.playerName", player.player_name) + prop("info.playGroup", "") +
                               prop("info.playGroupMaster", "") + prop("system.locale", system.locale) +
                               prop("hardware.videoOut.0.rotation", player.rotation.empty() ? "auto" : player.rotation);
            if (!player.reboot_days.empty() || !player.reboot_time.empty())
                text += prop("schedule.reboot.days", player.reboot_days) + prop("schedule.reboot.time", player.reboot_time);
            if (std::string level = soundLevel(player.volume); !level.empty())
                text += prop("hardware.audioOut.0.masterSoundLevel", level);
            if (!player.brightness.empty())
                text += prop("display.brightness", player.brightness);
            text += prop("time.timeZone", system.time_zone) + prop("content.serverUrl", player.index_uri);
            if (!system.device_manufacturer.empty())
                text += prop("garlic.device.manufacturer", system.device_manufacturer);
            for (const auto &[name, value] : system.own)
                text += prop(name, value);
            return text + "</userPref></configuration>";
        }
    }

    std::string playLogReport(std::string_view uuid, std::chrono::system_clock::time_point now, std::string_view lines)
    {
        return begin(uuid, now) + "<contentPlayLog>" + std::string(lines) + "</contentPlayLog>" + end();
    }

    std::string eventLogReport(std::string_view uuid, std::chrono::system_clock::time_point now, std::string_view lines)
    {
        return begin(uuid, now) + "<playerEventLog>" + std::string(lines) + "</playerEventLog>" + end();
    }

    std::string taskExecutionReport(std::string_view uuid, std::chrono::system_clock::time_point now,
                                    std::string_view tasks)
    {
        return begin(uuid, now) + "<taskState>" + std::string(tasks) + "</taskState>" + end();
    }

    std::string inventoryReport(std::string_view uuid, std::chrono::system_clock::time_point now, const SystemInfo &system,
                                const std::vector<InventoryObject> &objects)
    {
        std::string text = begin(uuid, now) + systemInfo(system) + "<contentDirectory><objectInfo>";
        for (const InventoryObject &object : objects)
            text += "<object id=\"" + escape(object.id) + "\" persistent=\"false\">" +
                    tag("resourceURI", object.resource_uri) + tag("state", object.state) +
                    tag("contentType", object.content_type) + tag("contentLength", std::to_string(object.content_length)) +
                    tag("lastModified", object.last_modified) + tag("expires", "") +
                    tag("transferLength", std::to_string(object.transfer_length)) + "</object>";
        return text + "</objectInfo></contentDirectory>" + end();
    }

    std::string systemReport(std::chrono::system_clock::time_point now, const SystemInfo &system,
                             const PlayerInfo &player)
    {
        return begin(player.uuid, now) + systemInfo(system, network(system) + configuration(system, player)) +
               "<hardwareInfo><hardware id=\"display:0\">" + prop("modelName", system.display_model) +
               prop("productId", system.display_product_id) + prop("serialNumber", system.display_serial) +
               prop("vendorId", system.display_vendor) + "</hardware></hardwareInfo>" + end();
    }
}
