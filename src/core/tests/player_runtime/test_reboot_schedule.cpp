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

#include "player_runtime/reboot_schedule.hpp"

using namespace garlic::core::player_runtime;
using namespace std::chrono;

namespace
{
    // Wednesday, 2026-10-28 10:00
    const local_seconds wednesday = local_days{October / 28 / 2026} + 10h;
}

TEST_CASE("the next of the given weekdays at the given time")
{
    CHECK(nextReboot("friday", "04:30:00", wednesday) == local_days{October / 30 / 2026} + 4h + 30min);
    CHECK(nextReboot("Monday FRIDAY", "04:30", wednesday) == local_days{October / 30 / 2026} + 4h + 30min);
}

TEST_CASE("today counts if the time has not passed, otherwise a week later")
{
    CHECK(nextReboot("wednesday", "12:00:00", wednesday) == local_days{October / 28 / 2026} + 12h);
    CHECK(nextReboot("wednesday", "09:00:00", wednesday) == local_days{November / 4 / 2026} + 9h);
}

TEST_CASE("invalid time: 03:00:00; no or unknown days: no restart")
{
    CHECK(nextReboot("thursday", "", wednesday) == local_days{October / 29 / 2026} + 3h);
    CHECK(nextReboot("thursday", "25:00", wednesday) == local_days{October / 29 / 2026} + 3h);
    CHECK_FALSE(nextReboot("", "03:00:00", wednesday).has_value());
    CHECK_FALSE(nextReboot("funday", "03:00:00", wednesday).has_value());
}

TEST_CASE("every weekday name, the earliest of several days counts")
{
    CHECK(nextReboot("sunday", "03:00", wednesday) == local_days{November / 1 / 2026} + 3h);
    CHECK(nextReboot("Monday", "03:00", wednesday) == local_days{November / 2 / 2026} + 3h);
    CHECK(nextReboot("tuesday", "03:00", wednesday) == local_days{November / 3 / 2026} + 3h);
    CHECK(nextReboot("saturday", "03:00", wednesday) == local_days{October / 31 / 2026} + 3h);
    CHECK(nextReboot("monday thursday sunday", "03:00", wednesday) == local_days{October / 29 / 2026} + 3h);
    CHECK(nextReboot("friday  thursday", "03:00", wednesday) == local_days{October / 29 / 2026} + 3h); // two spaces
}

TEST_CASE("today exactly at the time still counts")
{
    CHECK(nextReboot("wednesday", "10:00:00", wednesday) == wednesday);
}

TEST_CASE("the time: 23:59:59 is the last valid one, everything else is 03:00:00")
{
    CHECK(nextReboot("thursday", "23:59:59", wednesday) == local_days{October / 29 / 2026} + 23h + 59min + 59s);
    for (std::string_view invalid : {"24:00", "12:60", "12:00:60", "1:00", "12", "12:00:00:00", "ab:cd"})
    {
        CAPTURE(invalid);
        CHECK(nextReboot("thursday", invalid, wednesday) == local_days{October / 29 / 2026} + 3h);
    }
}
