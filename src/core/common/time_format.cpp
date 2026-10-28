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
#include "common/time_format.hpp"

#include <ctime>
#include <format>

namespace garlic::core::common
{
    namespace
    {
        // The time zone database of std::chrono is not available everywhere (ESP32, older Android), the C library is.
        std::tm localTm(std::time_t t)
        {
            std::tm local{};
#ifdef _WIN32
            localtime_s(&local, &t);
#else
            localtime_r(&t, &local);
#endif
            return local;
        }

        /// Offset of the local time zone at this moment, in seconds east of UTC.
        long utcOffset(std::time_t t, const std::tm &local)
        {
            using namespace std::chrono;
            auto local_as_utc = sys_days{year{local.tm_year + 1900} / (local.tm_mon + 1) / local.tm_mday} +
                                hours{local.tm_hour} + minutes{local.tm_min} + seconds{local.tm_sec};
            return static_cast<long>((local_as_utc - floor<seconds>(system_clock::from_time_t(t))).count());
        }
    }

    std::string isoLocalTime(std::chrono::system_clock::time_point time)
    {
        std::time_t t      = std::chrono::system_clock::to_time_t(time);
        std::tm     local  = localTm(t);
        long        offset = utcOffset(t, local);

        std::string text = std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}", local.tm_year + 1900, local.tm_mon + 1,
                                       local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);
        if (offset == 0)
            return text + "Z";

        char sign = offset < 0 ? '-' : '+';
        offset    = offset < 0 ? -offset : offset;
        return text + std::format("{}{:02}:{:02}", sign, offset / 3600, (offset % 3600) / 60);
    }

    std::string logTimestamp(std::chrono::system_clock::time_point time)
    {
        std::time_t t      = std::chrono::system_clock::to_time_t(time);
        std::tm     local  = localTm(t);
        auto        millis = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count() % 1000;

        return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02},{:03}", local.tm_year + 1900, local.tm_mon + 1,
                           local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec, millis);
    }
}
