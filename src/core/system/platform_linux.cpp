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
// Linux and Android
#include "system/platform.hpp"

#include "system/ifaddrs_network.hpp"

#include <net/if.h>
#include <net/if_arp.h>
#include <netpacket/packet.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <sstream>

#ifdef __ANDROID__
    #include <sys/system_properties.h>
#endif

namespace garlic::core::system::platform
{
    namespace
    {
        std::string readText(const char *path)
        {
            std::ifstream      file(path);
            std::ostringstream text;
            text << file.rdbuf();
            return text.str();
        }

        /// garlic-lib: Network::getType with the names of Qt's QNetworkInterface::InterfaceType
        std::string typeName(unsigned short hatype, const std::string &name)
        {
            switch (hatype)
            {
                case ARPHRD_ETHER:
                {
                    std::ifstream wireless("/sys/class/net/" + name + "/wireless");
                    return wireless.good() ? "wifi" : "ethernet";
                }
                case ARPHRD_LOOPBACK: return "loopback";
                case ARPHRD_PPP: return "Ppp";
                case ARPHRD_SLIP: return "slip";
#ifdef ARPHRD_CAN
                case ARPHRD_CAN: return "canbus";
#endif
#ifdef ARPHRD_PHONET
                case ARPHRD_PHONET: return "phonet";
#endif
#ifdef ARPHRD_IEEE802154
                case ARPHRD_IEEE802154: return "ieee802154";
#endif
                case ARPHRD_IEEE1394: return "firewire";
                case ARPHRD_NONE:
                case ARPHRD_TUNNEL:
                case ARPHRD_TUNNEL6:
                case ARPHRD_SIT: return "virtual";
                default: return "unknown";
            }
        }

#ifdef __ANDROID__
        std::string property(const char *name)
        {
            char value[PROP_VALUE_MAX] = {};
            __system_property_get(name, value);
            return value;
        }
#endif
    }

    Memory memory()
    {
        return parseMeminfo(readText("/proc/meminfo"));
    }

    ProcessInfo process()
    {
        return parseProcStatus(readText("/proc/self/status"));
    }

    /// The MAC comes with AF_PACKET (garlic-lib: QNetworkInterface).
    std::vector<reports::NetworkInterface> network()
    {
        return ifaddrsNetwork(AF_PACKET, [](const ifaddrs &ifa, reports::NetworkInterface &item)
        {
            const auto *ll = reinterpret_cast<const sockaddr_ll *>(ifa.ifa_addr);
            item.type      = typeName(ll->sll_hatype, item.id);
            item.mac       = macText(ll->sll_addr, ll->sll_halen);
        });
    }

    std::string timeZone()
    {
#ifdef __ANDROID__
        return property("persist.sys.timezone");
#else
        if (const char *tz = std::getenv("TZ"); tz && std::string_view(tz).contains('/'))
            return zoneName(tz);
        char    target[256] = {};
        ssize_t length      = readlink("/etc/localtime", target, sizeof(target) - 1);
        if (length > 0)
            return zoneName(std::string_view(target, static_cast<std::size_t>(length)));
        std::string name = readText("/etc/timezone"); // Debian without symlink
        while (!name.empty() && (name.back() == '\n' || name.back() == ' '))
            name.pop_back();
        return name;
#endif
    }

    std::string locale()
    {
#ifdef __ANDROID__
        return normalizeLocale(property("persist.sys.locale"));
#else
        for (const char *variable : {"LC_ALL", "LC_MESSAGES", "LANG"})
            if (const char *value = std::getenv(variable); value && *value)
                return normalizeLocale(value);
        return "C";
#endif
    }
}
