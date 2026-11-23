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
#include <time.h>
#include <unistd.h>

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#ifdef __ANDROID__
    #include <dlfcn.h>
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

    /**
     * CPU time: of the player and of all processes below it (the zygote, renderer and GPU processes of CEF), from
     * /proc/<pid>/stat. Android shows an app only its own processes; there it is the player process, without the
     * decoders of MediaCodec and the renderer of the WebView, which run in processes of the system.
     * The whole system: the first line of /proc/stat.
     */
    ProcessInfo process()
    {
        ProcessInfo info = parseProcStatus(readText("/proc/self/status"));

        std::map<int, ProcStat> processes;
        std::error_code         error;
        for (const auto &entry : std::filesystem::directory_iterator("/proc", error))
        {
            const std::string name = entry.path().filename().string();
            if (name.empty() || name.find_first_not_of("0123456789") != std::string::npos)
                continue;
            if (auto stat = parseProcStat(readText(("/proc/" + name + "/stat").c_str())))
                processes.emplace(std::stoi(name), *stat);
        }
        std::set<int> player{static_cast<int>(getpid())};
        for (bool grew = true; grew;) // the parents come in no particular order
        {
            grew = false;
            for (const auto &[pid, stat] : processes)
                if (player.contains(stat.parent) && player.insert(pid).second)
                    grew = true;
        }
        std::uint64_t ticks = 0;
        for (int pid : player)
            if (auto found = processes.find(pid); found != processes.end())
                ticks += found->second.ticks;
        long per_second = sysconf(_SC_CLK_TCK);
        if (processes.contains(static_cast<int>(getpid())) && per_second > 0)
            info.cpu_time = std::chrono::nanoseconds(ticks * (1'000'000'000ULL / static_cast<std::uint64_t>(per_second)));
        info.system_cpu = parseStatCpu(readText("/proc/stat")); // Android 8+: not readable, stays empty
        return info;
    }

    /**
     * Temperatures: every temp*_input of /sys/class/hwmon/<device> by its name, and the thermal zones by their type (Android
     * mostly has no hwmon). Throttling: on the Raspberry Pi "vcgencmd get_throttled" (/dev/vcio needs root), on Intel the
     * throttle counters of the kernel; AMD tells nothing. Android: the thermal status (NDK, Android 11+), loaded at run
     * time because the player runs from Android 7.1 on.
     */
    Thermal thermal()
    {
        Thermal result;
        auto    take = [&](std::string_view sensor, const std::string &text)
        {
            ThermalPart part = thermalPart(sensor);
            long long   milli = 0;
            auto        begin = text.data() + std::min(text.find_first_not_of(" \t"), text.size());
            if (part == ThermalPart::other || std::from_chars(begin, text.data() + text.size(), milli).ec != std::errc() ||
                milli <= 0) // 0 or below: a sensor that does not work
                return;
            auto                celsius = static_cast<int>(std::lround(static_cast<double>(milli) / 1000.0));
            std::optional<int> &slot    = part == ThermalPart::cpu ? result.cpu : part == ThermalPart::ram ? result.ram : result.ssd;
            slot                        = std::max(slot.value_or(celsius), celsius);
        };
        auto line = [](std::string text)
        {
            text.erase(std::min(text.find('\n'), text.size()));
            return text;
        };

        std::error_code error;
        for (const auto &device : std::filesystem::directory_iterator("/sys/class/hwmon", error))
        {
            std::string     name = line(readText((device.path() / "name").c_str()));
            std::error_code inner;
            for (const auto &file : std::filesystem::directory_iterator(device.path(), inner))
            {
                std::string file_name = file.path().filename().string();
                if (file_name.starts_with("temp") && file_name.ends_with("_input"))
                    take(name, readText(file.path().c_str()));
            }
        }
        for (const auto &zone : std::filesystem::directory_iterator("/sys/class/thermal", error))
            if (zone.path().filename().string().starts_with("thermal_zone"))
                take(line(readText((zone.path() / "type").c_str())), readText((zone.path() / "temp").c_str()));

#ifdef __ANDROID__
        // AThermal_*: ATHERMAL_STATUS_LIGHT (1) is "light throttling" already
        static void *manager            = nullptr;
        static int (*current)(void *)   = nullptr;
        [[maybe_unused]] static bool loaded = []
        {
            if (void *android = dlopen("libandroid.so", RTLD_NOW))
            {
                auto acquire = reinterpret_cast<void *(*)()>(dlsym(android, "AThermal_acquireManager"));
                current      = reinterpret_cast<int (*)(void *)>(dlsym(android, "AThermal_getCurrentThermalStatus"));
                if (acquire != nullptr && current != nullptr)
                    manager = acquire(); // kept for the whole run
            }
            return true;
        }();
        if (manager != nullptr)
            if (int value = current(manager); value >= 0)
                result.throttling = value >= 1;
#else
        if (std::filesystem::exists("/usr/bin/vcgencmd", error))
            if (FILE *pipe = popen("/usr/bin/vcgencmd get_throttled 2>/dev/null", "r"))
            {
                char buffer[64] = {};
                std::string output(buffer, std::fread(buffer, 1, sizeof(buffer) - 1, pipe));
                pclose(pipe);
                result.throttling = parseThrottled(output);
            }
        for (const auto &cpu : std::filesystem::directory_iterator("/sys/devices/system/cpu", error))
            for (const char *counter : {"core_throttle_count", "package_throttle_count"})
            {
                std::string   text  = readText((cpu.path() / "thermal_throttle" / counter).c_str());
                std::uint64_t value = 0;
                if (!text.empty() && std::from_chars(text.data(), text.data() + text.size(), value).ec == std::errc())
                    result.throttle_events = result.throttle_events.value_or(0) + value;
            }
#endif
        return result;
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

    /// CLOCK_BOOTTIME: since the boot with the time in suspend
    std::optional<std::chrono::seconds> uptime()
    {
        timespec now{};
        if (clock_gettime(CLOCK_BOOTTIME, &now) != 0)
            return {};
        return std::chrono::seconds(now.tv_sec);
    }

    /**
     * Android: the build properties. Linux: the device tree (Raspberry Pi and other ARM boards, "Raspberry Pi 4 Model B
     * Rev 1.4"), else the DMI of the BIOS (PCs).
     */
    Device device()
    {
#ifdef __ANDROID__
        return {property("ro.product.model"), property("ro.product.manufacturer")};
#else
        auto clean = [](std::string text)
        {
            while (!text.empty() && (text.back() == '\0' || text.back() == '\n' || text.back() == ' '))
                text.pop_back();
            return text;
        };
        if (std::string model = clean(readText("/sys/firmware/devicetree/base/model")); !model.empty())
            return {model, {}};
        return {clean(readText("/sys/class/dmi/id/product_name")), clean(readText("/sys/class/dmi/id/sys_vendor"))};
#endif
    }
}
