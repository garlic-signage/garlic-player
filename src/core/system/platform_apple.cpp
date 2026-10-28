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
// macOS and iOS - not compiled on the development machine yet, to be checked with the first Apple build
#include "system/platform.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <mach/mach.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <sys/sysctl.h>

#include <format>
#include <map>

namespace garlic::core::system::platform
{
    namespace
    {
        std::string utf8(CFStringRef text)
        {
            if (text == nullptr)
                return {};
            char buffer[256] = {};
            if (!CFStringGetCString(text, buffer, sizeof(buffer), kCFStringEncodingUTF8))
                return {};
            return buffer;
        }

        std::string address(const sockaddr *sa)
        {
            char buffer[INET6_ADDRSTRLEN] = {};
            if (sa == nullptr)
                return {};
            if (sa->sa_family == AF_INET)
                inet_ntop(AF_INET, &reinterpret_cast<const sockaddr_in *>(sa)->sin_addr, buffer, sizeof(buffer));
            else if (sa->sa_family == AF_INET6)
                inet_ntop(AF_INET6, &reinterpret_cast<const sockaddr_in6 *>(sa)->sin6_addr, buffer, sizeof(buffer));
            return buffer;
        }

        std::string typeName(unsigned char type)
        {
            switch (type)
            {
                case IFT_ETHER: return "ethernet";
                case IFT_LOOP: return "loopback";
                case IFT_PPP: return "Ppp";
                case IFT_GIF:
                case IFT_STF: return "virtual";
                case IFT_IEEE1394: return "firewire";
                default: return "unknown";
            }
        }
    }

    /// available = free + inactive + purgeable pages, like the activity monitor (garlic-lib took only free pages)
    Memory memory()
    {
        Memory        result;
        std::uint64_t total  = 0;
        size_t        length = sizeof(total);
        if (sysctlbyname("hw.memsize", &total, &length, nullptr, 0) == 0)
            result.total = total;

        vm_size_t              page_size = 0;
        vm_statistics64_data_t vm{};
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        mach_port_t            host  = mach_host_self();
        if (host_page_size(host, &page_size) == KERN_SUCCESS &&
            host_statistics64(host, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm), &count) == KERN_SUCCESS)
            result.available = (static_cast<std::uint64_t>(vm.free_count) + vm.inactive_count + vm.purgeable_count) * page_size;
        return result;
    }

    ProcessInfo process()
    {
        ProcessInfo                 info;
        mach_task_basic_info_data_t basic{};
        mach_msg_type_number_t      count = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&basic), &count) == KERN_SUCCESS)
            info.memory = basic.resident_size;

        thread_act_array_t     threads      = nullptr;
        mach_msg_type_number_t thread_count = 0;
        if (task_threads(mach_task_self(), &threads, &thread_count) == KERN_SUCCESS)
        {
            info.threads = static_cast<int>(thread_count);
            for (mach_msg_type_number_t i = 0; i < thread_count; ++i)
                mach_port_deallocate(mach_task_self(), threads[i]);
            vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(threads), thread_count * sizeof(thread_act_t));
        }
        return info;
    }

    /// getifaddrs: the MAC comes with AF_LINK, the addresses with AF_INET and AF_INET6.
    std::vector<reports::NetworkInterface> network()
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
            if (family == AF_LINK)
            {
                const auto *dl   = reinterpret_cast<const sockaddr_dl *>(ifa->ifa_addr);
                auto       &item = entry(name);
                item.type        = typeName(dl->sdl_type);
                const auto *mac  = reinterpret_cast<const unsigned char *>(LLADDR(dl));
                std::string text;
                for (int i = 0; i < dl->sdl_alen; ++i)
                    text += std::format("{}{:02X}", i ? ":" : "", mac[i]);
                item.mac = text;
            }
            else if (family == AF_INET || family == AF_INET6)
            {
                reports::NetworkAddress a;
                a.family  = family == AF_INET ? reports::NetworkAddress::Family::V4 : reports::NetworkAddress::Family::V6;
                a.ip      = address(ifa->ifa_addr);
                a.netmask = address(ifa->ifa_netmask);
                if (family == AF_INET && (ifa->ifa_flags & IFF_BROADCAST))
                    a.broadcast = address(ifa->ifa_dstaddr);
                entry(name).addresses.push_back(std::move(a));
            }
        }
        freeifaddrs(list);
        std::erase_if(interfaces, [](const auto &i) { return i.mac.empty() || i.addresses.empty(); });
        return interfaces;
    }

    std::string timeZone()
    {
        CFTimeZoneRef zone = CFTimeZoneCopySystem();
        std::string   name = utf8(CFTimeZoneGetName(zone));
        CFRelease(zone);
        return name;
    }

    std::string locale()
    {
        CFLocaleRef current = CFLocaleCopyCurrent();
        std::string name    = utf8(CFLocaleGetIdentifier(current));
        CFRelease(current);
        return normalizeLocale(name);
    }
}
