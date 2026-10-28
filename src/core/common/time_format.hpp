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
#ifndef GARLIC_CORE_COMMON_TIME_FORMAT_HPP
#define GARLIC_CORE_COMMON_TIME_FORMAT_HPP

#include <chrono>
#include <string>

namespace garlic::core::common
{
    /// Local time with UTC offset like Qt::ISODate: "2026-10-28T03:30:56+01:00", "…Z" at offset 0.
    std::string isoLocalTime(std::chrono::system_clock::time_point time);

    /// Local time with milliseconds for text logs: "2026-10-28 03:30:56,123".
    std::string logTimestamp(std::chrono::system_clock::time_point time);

    /// The local wall time of a moment, e.g. for SMIL wallclock values. Uses the C library, not the tz database.
    std::chrono::local_seconds toLocalTime(std::chrono::system_clock::time_point time);
}

#endif // GARLIC_CORE_COMMON_TIME_FORMAT_HPP
