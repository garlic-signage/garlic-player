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
#include "system/system_info.hpp"

#include "system/platform.hpp"

#include <charconv>
#include <filesystem>

namespace garlic::core::system
{
    namespace
    {
        /// The number after "Key:" in a /proc file, e.g. "MemTotal:  16314508 kB".
        std::optional<std::uint64_t> field(std::string_view text, std::string_view key)
        {
            for (std::size_t start = 0; start < text.size();)
            {
                auto end  = text.find('\n', start);
                auto line = text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
                if (line.starts_with(key) && line.size() > key.size() && line[key.size()] == ':')
                {
                    auto          digits = line.find_first_of("0123456789", key.size());
                    std::uint64_t value  = 0;
                    if (digits == std::string_view::npos ||
                        std::from_chars(line.data() + digits, line.data() + line.size(), value).ec != std::errc())
                        return std::nullopt;
                    return value;
                }
                if (end == std::string_view::npos)
                    break;
                start = end + 1;
            }
            return std::nullopt;
        }

        std::optional<std::uint64_t> kilobytes(std::string_view text, std::string_view key)
        {
            auto value = field(text, key);
            return value ? std::optional(*value * 1024) : std::nullopt;
        }
    }

    Memory parseMeminfo(std::string_view text)
    {
        Memory memory;
        memory.total     = kilobytes(text, "MemTotal");
        memory.available = kilobytes(text, "MemAvailable");
        if (!memory.available)
            memory.available = kilobytes(text, "MemFree");
        return memory;
    }

    ProcessInfo parseProcStatus(std::string_view text)
    {
        ProcessInfo info;
        info.memory  = kilobytes(text, "VmRSS").value_or(0);
        info.threads = static_cast<int>(field(text, "Threads").value_or(0));
        return info;
    }

    std::string normalizeLocale(std::string_view value)
    {
        value = value.substr(0, value.find_first_of(".@"));
        if (value.empty() || value == "POSIX" || value == "C")
            return "C";
        std::string name(value);
        for (char &c : name)
            if (c == '-')
                c = '_';
        return name;
    }

    std::string zoneName(std::string_view value)
    {
        if (value.starts_with(':'))
            value.remove_prefix(1);
        if (auto at = value.find("zoneinfo/"); at != std::string_view::npos)
            value.remove_prefix(at + 9);
        return std::string(value);
    }

    DiskSpace diskSpace(std::string_view cache_dir)
    {
        std::error_code ec;
        auto            space = std::filesystem::space(std::filesystem::path(std::u8string(cache_dir.begin(), cache_dir.end())), ec);
        if (ec)
            return {};
        // garlic-lib: bytes_total = 96 %, bytes_locked = the rest, free = available - locked
        std::uint64_t total    = space.capacity / 100 * 96;
        std::uint64_t reserved = space.capacity - total;
        return {total, space.available > reserved ? space.available - reserved : 0};
    }

    ProcessInfo processInfo()
    {
        return platform::process();
    }

    reports::SystemInfo collectSystemInfo(std::string_view cache_dir, std::string_view start_time)
    {
        reports::SystemInfo info;
        info.start_time = start_time;
        info.time_zone  = platform::timeZone();
        info.locale     = platform::locale();

        DiskSpace disk = diskSpace(cache_dir);
        if (disk.total > 0)
        {
            info.total_capacity = disk.total;
            info.free_space     = disk.free;
        }

        // used = total - available (garlic-lib took MemFree, so the file cache of the kernel counted as used)
        Memory memory = platform::memory();
        info.memory_total = memory.total;
        if (memory.total && memory.available)
            info.memory_used = *memory.total > *memory.available ? *memory.total - *memory.available : 0;

        info.network = platform::network();
        return info;
    }
}
