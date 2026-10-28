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
#include <doctest/doctest.h>

#include "common/time_format.hpp"
#include "time_zone.hpp"

using namespace garlic::core::common;
using garlic::core::tests::TimeZone;
using namespace std::chrono;

namespace
{
    const auto new_year = sys_days{January / 1 / 2026} + 30min;
}

TEST_CASE("isoLocalTime writes the local time with offset like Qt::ISODate")
{
    {
        TimeZone tz("UTC");
        CHECK(isoLocalTime(new_year) == "2026-01-01T00:30:00Z");
    }
    {
        TimeZone tz("CET-1"); // UTC+1
        CHECK(isoLocalTime(new_year) == "2026-01-01T01:30:00+01:00");
    }
    {
        TimeZone tz("EST5"); // UTC-5, the day before
        CHECK(isoLocalTime(new_year) == "2025-12-31T19:30:00-05:00");
    }
    {
        TimeZone tz("IST-5:30"); // UTC+5:30
        CHECK(isoLocalTime(new_year) == "2026-01-01T06:00:00+05:30");
    }
}

TEST_CASE("logTimestamp writes local time with milliseconds")
{
    TimeZone tz("UTC");
    CHECK(logTimestamp(new_year + 1234ms) == "2026-01-01 00:30:01,234");
}

TEST_CASE("toLocalTime gives the local wall time")
{
    {
        TimeZone tz("UTC");
        CHECK(toLocalTime(new_year) == local_days{January / 1 / 2026} + 30min);
    }
    {
        TimeZone tz("EST5"); // UTC-5, the day before
        CHECK(toLocalTime(new_year) == local_days{December / 31 / 2025} + 19h + 30min);
    }
}
