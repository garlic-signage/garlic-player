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
                   tag("date", std::format("{:%Y-%m-%dT%H:%M:%S}Z", utc)) + tag("version", "1.0") + "<player id=\"" +
                   escape(uuid) + "\">";
        }

        std::string end()
        {
            return "</player></report>";
        }

        /// garlic-lib: CreateBase::createSystemInfo; inner is placed before </systemInfo> (network, configuration).
        std::string systemInfo(const SystemInfo &system, std::string_view inner = {})
        {
            return "<systemInfo>" + tag("systemStartTime", system.start_time) + tag("systemTimeZone", system.time_zone) +
                   tag("totalCapacity", number(system.total_capacity)) + tag("totalFreeSpace", number(system.free_space)) +
                   tag("cpuUsage", "") + tag("memoryTotal", number(system.memory_total)) +
                   tag("memoryUsed", number(system.memory_used)) + tag("hdmiOutput", "") + std::string(inner) +
                   "</systemInfo>";
        }

        /// garlic-lib: CreateSystemReport::createNetwork
        std::string network(const SystemInfo &system)
        {
            std::string text = "<network>";
            for (const NetworkInterface &interface : system.network)
            {
                text += "<interface id=\"" + escape(interface.id) + "\">" + tag("mac", interface.mac) +
                        tag("type", interface.type);
                for (const NetworkAddress &address : interface.addresses)
                {
                    if (address.family == NetworkAddress::Family::V4)
                        text += tag("ip_v4", address.ip) + tag("netmask_v4", address.netmask) +
                                tag("broadcast_v4", address.broadcast);
                    else
                        text += tag("ip_v6", address.ip) + tag("netmask_v6", address.netmask);
                }
                text += "</interface>";
            }
            return text + "</network>";
        }

        /// garlic-lib: CreateSystemReport::createConfiguration with modelInfo, factoryDefault, userPref
        std::string configuration(const SystemInfo &system, const PlayerInfo &player)
        {
            return "<configuration><modelInfo>" + prop("PCB", player.app_name) + prop("PCBRevision", "") +
                   prop("operatingSystem", player.os) + prop("manufacturer", "Sagiadinos") +
                   prop("manufacturerURL", "https://garlic-player.com") + prop("modelDescription", "") +
                   prop("modelName", "") + prop("modelURL", "") + prop("option", "") + "</modelInfo>" +
                   "<factoryDefault>" + tag("variant", "") + "</factoryDefault><userPref>" +
                   prop("info.playerName", player.player_name) + prop("info.playGroup", "") +
                   prop("info.playGroupMaster", "") + prop("system.locale", system.locale) +
                   prop("content.serverUrl", player.index_uri) + "</userPref></configuration>";
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
               "<hardwareInfo><hardware id=\"display:0\">" + prop("modelName", "") + prop("product_id", "") +
               prop("serialNumber", "") + prop("vendorId", "") + "</hardware></hardwareInfo>" + end();
    }
}
