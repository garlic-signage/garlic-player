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

#include "system/ifaddrs_network.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <sys/sysctl.h>

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

    /// The MAC comes with AF_LINK.
    std::vector<reports::NetworkInterface> network()
    {
        return ifaddrsNetwork(AF_LINK, [](const ifaddrs &ifa, reports::NetworkInterface &item)
        {
            const auto *dl = reinterpret_cast<const sockaddr_dl *>(ifa.ifa_addr);
            item.type      = typeName(dl->sdl_type);
            item.mac       = macText(reinterpret_cast<const unsigned char *>(LLADDR(dl)), dl->sdl_alen);
        });
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
