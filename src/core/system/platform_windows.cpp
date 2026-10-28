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
// Windows - not compiled on the development machine yet, to be checked with the first Windows build
#include "system/platform.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <icu.h>

#include <format>
#include <vector>

namespace garlic::core::system::platform
{
    namespace
    {
        std::string utf8(const wchar_t *text)
        {
            int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
            if (size <= 1)
                return {};
            std::string result(static_cast<std::size_t>(size - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);
            return result;
        }

        std::string address(const void *raw, int family)
        {
            char buffer[INET6_ADDRSTRLEN] = {};
            inet_ntop(family, raw, buffer, sizeof(buffer));
            return buffer;
        }

        std::string typeName(IFTYPE type)
        {
            switch (type)
            {
                case IF_TYPE_ETHERNET_CSMACD: return "ethernet";
                case IF_TYPE_IEEE80211: return "wifi";
                case IF_TYPE_SOFTWARE_LOOPBACK: return "loopback";
                case IF_TYPE_PPP: return "Ppp";
                case IF_TYPE_TUNNEL: return "virtual";
                case IF_TYPE_IEEE1394: return "firewire";
                default: return "unknown";
            }
        }
    }

    Memory memory()
    {
        MEMORYSTATUSEX status{};
        status.dwLength = sizeof(status);
        if (!GlobalMemoryStatusEx(&status))
            return {};
        return {status.ullTotalPhys, status.ullAvailPhys};
    }

    ProcessInfo process()
    {
        ProcessInfo             info;
        PROCESS_MEMORY_COUNTERS counters{};
        if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
            info.memory = counters.WorkingSetSize;

        // garlic-lib: General::countThreads
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32 entry{};
            entry.dwSize = sizeof(entry);
            DWORD self   = GetCurrentProcessId();
            for (BOOL ok = Process32First(snapshot, &entry); ok; ok = Process32Next(snapshot, &entry))
                if (entry.th32ProcessID == self)
                {
                    info.threads = static_cast<int>(entry.cntThreads);
                    break;
                }
            CloseHandle(snapshot);
        }
        return info;
    }

    std::vector<reports::NetworkInterface> network()
    {
        ULONG             size = 16 * 1024;
        std::vector<BYTE> buffer(size);
        auto             *adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data());
        ULONG result = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapters, &size);
        if (result == ERROR_BUFFER_OVERFLOW)
        {
            buffer.resize(size);
            adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data());
            result   = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapters, &size);
        }
        if (result != NO_ERROR)
            return {};

        std::vector<reports::NetworkInterface> interfaces;
        for (auto *adapter = adapters; adapter != nullptr; adapter = adapter->Next)
        {
            reports::NetworkInterface item;
            char                      name[NDIS_IF_MAX_STRING_SIZE + 1] = {};
            if (ConvertInterfaceLuidToNameA(&adapter->Luid, name, sizeof(name)) == NO_ERROR)
                item.id = name; // like Qt: "ethernet_32769"
            item.type = typeName(adapter->IfType);
            for (ULONG i = 0; i < adapter->PhysicalAddressLength; ++i)
                item.mac += std::format("{}{:02X}", i ? ":" : "", adapter->PhysicalAddress[i]);

            for (auto *unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next)
            {
                const sockaddr         *sa = unicast->Address.lpSockaddr;
                reports::NetworkAddress a;
                if (sa->sa_family == AF_INET)
                {
                    const auto &in = reinterpret_cast<const sockaddr_in *>(sa)->sin_addr;
                    ULONG       mask = 0;
                    ConvertLengthToIpv4Mask(unicast->OnLinkPrefixLength, &mask);
                    in_addr broadcast{};
                    broadcast.S_un.S_addr = in.S_un.S_addr | ~mask;
                    in_addr netmask{};
                    netmask.S_un.S_addr = mask;
                    a.family            = reports::NetworkAddress::Family::V4;
                    a.ip                = address(&in, AF_INET);
                    a.netmask           = address(&netmask, AF_INET);
                    a.broadcast         = address(&broadcast, AF_INET);
                }
                else if (sa->sa_family == AF_INET6)
                {
                    in6_addr mask{};
                    for (UINT8 bit = 0; bit < unicast->OnLinkPrefixLength && bit < 128; ++bit)
                        mask.s6_addr[bit / 8] |= static_cast<UCHAR>(0x80 >> (bit % 8));
                    a.family  = reports::NetworkAddress::Family::V6;
                    a.ip      = address(&reinterpret_cast<const sockaddr_in6 *>(sa)->sin6_addr, AF_INET6);
                    a.netmask = address(&mask, AF_INET6);
                }
                else
                    continue;
                item.addresses.push_back(std::move(a));
            }
            if (!item.mac.empty() && !item.addresses.empty())
                interfaces.push_back(std::move(item));
        }
        return interfaces;
    }

    /// The IANA name like Qt ("Europe/Berlin"), through the ICU of Windows 10; otherwise the Windows name.
    std::string timeZone()
    {
        DYNAMIC_TIME_ZONE_INFORMATION zone{};
        GetDynamicTimeZoneInformation(&zone);
        UChar      id[128] = {};
        UErrorCode status  = U_ZERO_ERROR;
        int32_t    length  = ucal_getTimeZoneIDForWindowsID(reinterpret_cast<const UChar *>(zone.TimeZoneKeyName), -1,
                                                            nullptr, id, 128, &status);
        if (U_SUCCESS(status) && length > 0)
            return utf8(reinterpret_cast<const wchar_t *>(id));
        return utf8(zone.TimeZoneKeyName);
    }

    std::string locale()
    {
        wchar_t name[LOCALE_NAME_MAX_LENGTH] = {};
        if (GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH) == 0)
            return "C";
        return normalizeLocale(utf8(name));
    }
}
