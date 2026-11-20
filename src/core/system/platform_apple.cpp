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
// macOS and iOS - compiled by the macOS build of build.yml, not by the CI in Debian Trixie
#include "system/platform.hpp"

#include "system/ifaddrs_network.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <TargetConditionals.h>
#include <mach/mach.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <sys/resource.h>
#include <unistd.h>
#if TARGET_OS_OSX
    #include <libproc.h>
    #include <mach/mach_time.h>
#endif
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <sys/sysctl.h>

#include <vector>

namespace garlic::core::system::platform
{
    namespace
    {
#if TARGET_OS_OSX
        /// CPU time of the processes below parent, recursively; proc_pid_rusage counts in mach time units (Apple
        /// Silicon: not nanoseconds)
        std::chrono::nanoseconds childrenCpuTime(pid_t parent)
        {
            static const mach_timebase_info_data_t timebase = []
            {
                mach_timebase_info_data_t value{};
                mach_timebase_info(&value);
                return value;
            }();
            std::vector<pid_t> children(256);
            int count = proc_listchildpids(parent, children.data(), static_cast<int>(children.size() * sizeof(pid_t)));
            std::chrono::nanoseconds total{0};
            for (int i = 0; i < count && i < static_cast<int>(children.size()); ++i)
            {
                rusage_info_v2 usage{};
                if (proc_pid_rusage(children[i], RUSAGE_INFO_V2, reinterpret_cast<rusage_info_t *>(&usage)) == 0 &&
                    timebase.denom != 0)
                    total += std::chrono::nanoseconds((usage.ri_user_time + usage.ri_system_time) * timebase.numer /
                                                      timebase.denom);
                total += childrenCpuTime(children[i]);
            }
            return total;
        }
#endif

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

        rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) == 0)
        {
            auto time = [](const timeval &value)
            { return std::chrono::seconds(value.tv_sec) + std::chrono::microseconds(value.tv_usec); };
            info.cpu_time = time(usage.ru_utime) + time(usage.ru_stime);
#if TARGET_OS_OSX
            // the helper processes of CEF; VideoToolbox decodes partly in a service of the system, not counted
            *info.cpu_time += childrenCpuTime(getpid());
#endif
        }

        host_cpu_load_info_data_t load{};
        mach_msg_type_number_t    load_count = HOST_CPU_LOAD_INFO_COUNT;
        if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, reinterpret_cast<host_info_t>(&load), &load_count) ==
            KERN_SUCCESS)
        {
            std::uint64_t busy = static_cast<std::uint64_t>(load.cpu_ticks[CPU_STATE_USER]) +
                                 load.cpu_ticks[CPU_STATE_SYSTEM] + load.cpu_ticks[CPU_STATE_NICE];
            info.system_cpu = SystemCpu{busy, busy + load.cpu_ticks[CPU_STATE_IDLE]};
        }
        return info;
    }

    /// No temperatures (only through the private SMC). Throttling: NSProcessInfo.thermalState serious (2) or critical
    /// (3); fair is "slightly elevated" only. Through the Objective-C runtime, this file is C++.
    Thermal thermal()
    {
        Thermal result;
        id      info = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(reinterpret_cast<id>(objc_getClass("NSProcessInfo")),
                                                                 sel_registerName("processInfo"));
        if (info != nullptr)
            result.throttling = reinterpret_cast<long (*)(id, SEL)>(objc_msgSend)(info, sel_registerName("thermalState")) >= 2;
        return result;
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
