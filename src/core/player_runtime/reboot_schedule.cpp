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
#include "player_runtime/reboot_schedule.hpp"

#include <array>
#include <charconv>

namespace garlic::core::player_runtime
{
    namespace
    {
        /// 1 = Monday … 7 = Sunday, 0 if unknown (garlic-lib: WeekdayConverter::convertWeekDay)
        unsigned weekday(std::string_view name)
        {
            static constexpr std::array<std::string_view, 7> names = {"monday", "tuesday", "wednesday", "thursday",
                                                                      "friday", "saturday", "sunday"};
            std::string lower(name);
            for (char &c : lower)
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
            for (unsigned i = 0; i < names.size(); ++i)
                if (lower == names[i])
                    return i + 1;
            return 0;
        }

        /// "03:00:00" or "03:00" → seconds since midnight; nullopt if invalid (garlic-lib: QTime::fromString)
        std::optional<std::chrono::seconds> timeOfDay(std::string_view text)
        {
            int  parts[3] = {0, 0, 0};
            int  count    = 0;
            auto rest     = text;
            while (!rest.empty() && count < 3)
            {
                auto colon = rest.find(':');
                auto part  = rest.substr(0, colon);
                if (part.size() != 2 ||
                    std::from_chars(part.data(), part.data() + part.size(), parts[count]).ec != std::errc())
                    return std::nullopt;
                ++count;
                if (colon == std::string_view::npos)
                    rest = {};
                else
                    rest.remove_prefix(colon + 1);
            }
            if (count < 2 || !rest.empty() || parts[0] > 23 || parts[1] > 59 || parts[2] > 59)
                return std::nullopt;
            return std::chrono::hours(parts[0]) + std::chrono::minutes(parts[1]) + std::chrono::seconds(parts[2]);
        }
    }

    std::optional<std::chrono::local_seconds> nextReboot(std::string_view days, std::string_view time,
                                                         std::chrono::local_seconds now)
    {
        using namespace std::chrono;
        seconds    at    = timeOfDay(time).value_or(hours(3));
        local_days today = floor<std::chrono::days>(now);
        unsigned   wd    = std::chrono::weekday{today}.iso_encoding();

        std::optional<local_seconds> earliest;
        std::size_t                  start = 0;
        while (start < days.size())
        {
            auto end = days.find(' ', start);
            auto day = weekday(days.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
            if (day > 0)
            {
                int add = static_cast<int>((day + 7 - wd) % 7);
                if (add == 0 && now > today + at) // today, but the time has passed
                    add = 7;
                local_seconds candidate = today + std::chrono::days(add) + at;
                if (!earliest || candidate < *earliest)
                    earliest = candidate;
            }
            if (end == std::string_view::npos)
                break;
            start = end + 1;
        }
        return earliest;
    }
}
