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
#include "system/ifaddrs_network.hpp"

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>

#include <format>
#include <map>

namespace garlic::core::system::platform
{
    namespace
    {
        std::string address(const sockaddr *sa, const char *interface_name = nullptr)
        {
            char buffer[INET6_ADDRSTRLEN] = {};
            if (sa == nullptr)
                return {};
            if (sa->sa_family == AF_INET)
            {
                inet_ntop(AF_INET, &reinterpret_cast<const sockaddr_in *>(sa)->sin_addr, buffer, sizeof(buffer));
                return buffer;
            }
            if (sa->sa_family == AF_INET6)
            {
                const auto *in6 = reinterpret_cast<const sockaddr_in6 *>(sa);
                inet_ntop(AF_INET6, &in6->sin6_addr, buffer, sizeof(buffer));
                std::string text = buffer;
                if (in6->sin6_scope_id != 0 && interface_name != nullptr)
                    text += std::string("%") + interface_name; // like QHostAddress for link-local addresses
                return text;
            }
            return {};
        }
    }

    std::string macText(const unsigned char *bytes, int length)
    {
        std::string text;
        for (int i = 0; i < length; ++i)
            text += std::format("{}{:02X}", i ? ":" : "", bytes[i]);
        return text;
    }

    std::vector<reports::NetworkInterface> ifaddrsNetwork(int link_family, LinkLayer link_layer)
    {
        ifaddrs *list = nullptr;
        if (getifaddrs(&list) != 0)
            return {};

        std::vector<reports::NetworkInterface> interfaces;
        std::map<std::string, std::size_t>     index;
        auto entry = [&](const std::string &name) -> reports::NetworkInterface &
        {
            auto [it, added] = index.try_emplace(name, interfaces.size());
            if (added)
                interfaces.push_back({name, {}, "unknown", {}});
            return interfaces[it->second];
        };

        for (ifaddrs *ifa = list; ifa != nullptr; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == nullptr || ifa->ifa_name == nullptr)
                continue;
            std::string name   = ifa->ifa_name;
            int         family = ifa->ifa_addr->sa_family;
            if (family == link_family)
                link_layer(*ifa, entry(name));
            else if (family == AF_INET || family == AF_INET6)
            {
                reports::NetworkAddress a;
                a.family  = family == AF_INET ? reports::NetworkAddress::Family::V4 : reports::NetworkAddress::Family::V6;
                a.ip      = address(ifa->ifa_addr, ifa->ifa_name);
                a.netmask = address(ifa->ifa_netmask);
                if (family == AF_INET && (ifa->ifa_flags & IFF_BROADCAST))
                    a.broadcast = address(ifa->ifa_broadaddr); // on Apple a macro for ifa_dstaddr
                entry(name).addresses.push_back(std::move(a));
            }
        }
        freeifaddrs(list);

        // garlic-lib: isRealInterface - a MAC and at least one address
        std::erase_if(interfaces, [](const auto &i) { return i.mac.empty() || i.addresses.empty(); });
        return interfaces;
    }
}
