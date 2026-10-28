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

#include "smil/wallclock.hpp"

using namespace garlic::core::smil;
using namespace std::chrono;

namespace
{
    Wallclock wallclock(local_seconds start, int repeats = 0, Period period = {})
    {
        return Wallclock{start, repeats, period};
    }

    Period daily()
    {
        return Period{0, 0, 1, seconds(0)};
    }

    const local_seconds jan1 = local_days{January / 1 / 2026} + 8h;
}

TEST_CASE("before the start: no previous, next is the start")
{
    auto triggers = wallclockTriggers(wallclock(jan1, -1, daily()), jan1 - 1s);
    CHECK_FALSE(triggers.previous.has_value());
    CHECK(triggers.next == jan1);
}

TEST_CASE("without repeat: once at the start")
{
    auto at_start = wallclockTriggers(wallclock(jan1), jan1);
    CHECK(at_start.previous == jan1); // previous is <= now: fire now
    CHECK_FALSE(at_start.next.has_value());

    auto later = wallclockTriggers(wallclock(jan1), jan1 + 1000h);
    CHECK(later.previous == jan1);
    CHECK_FALSE(later.next.has_value());
}

TEST_CASE("R5: the start and 5 repeats")
{
    auto w = wallclock(jan1, 5, daily());

    auto day4 = wallclockTriggers(w, local_days{January / 4 / 2026} + 9h);
    CHECK(day4.previous == local_days{January / 4 / 2026} + 8h);
    CHECK(day4.next == local_days{January / 5 / 2026} + 8h);

    auto day6 = wallclockTriggers(w, local_days{January / 6 / 2026} + 9h); // the 5th repeat was the last
    CHECK(day6.previous == local_days{January / 6 / 2026} + 8h);
    CHECK_FALSE(day6.next.has_value());

    auto day10 = wallclockTriggers(w, local_days{January / 10 / 2026});
    CHECK(day10.previous == local_days{January / 6 / 2026} + 8h);
    CHECK_FALSE(day10.next.has_value());
}

TEST_CASE("endless short period over years, without stepping through every period")
{
    auto w   = wallclock(local_days{April / 1 / 2021}, -1, Period{0, 0, 0, seconds(15)});
    auto now = local_days{October / 28 / 2026} + 8h + 7s;

    auto triggers = wallclockTriggers(w, now);
    CHECK(triggers.previous == local_days{October / 28 / 2026} + 8h);
    CHECK(triggers.next == local_days{October / 28 / 2026} + 8h + 15s);

    auto exactly = wallclockTriggers(w, local_days{October / 28 / 2026} + 8h + 15s);
    CHECK(exactly.previous == local_days{October / 28 / 2026} + 8h + 15s);
    CHECK(exactly.next == local_days{October / 28 / 2026} + 8h + 30s);
}

TEST_CASE("months and years: the day is clamped to the end of the month like Qt")
{
    auto monthly = wallclock(local_days{January / 31 / 2026} + 8h, -1, Period{0, 1, 0, seconds(0)});
    auto feb     = wallclockTriggers(monthly, local_days{February / 28 / 2026} + 12h);
    CHECK(feb.previous == local_days{February / 28 / 2026} + 8h);
    CHECK(feb.next == local_days{March / 31 / 2026} + 8h); // calculated from the start, not from 28.02.

    auto yearly = wallclock(local_days{February / 29 / 2024}, -1, Period{1, 0, 0, seconds(0)});
    auto y2025  = wallclockTriggers(yearly, local_days{June / 1 / 2025});
    CHECK(y2025.previous == local_days{February / 28 / 2025});
    CHECK(y2025.next == local_days{February / 28 / 2026});
}

TEST_CASE("weekly with time: every Monday 07:30")
{
    auto w = wallclock(local_days{January / 5 / 2026} + 7h + 30min, -1, Period{0, 0, 7, seconds(0)});
    auto triggers = wallclockTriggers(w, local_days{October / 28 / 2026} + 12h); // a Wednesday
    CHECK(triggers.previous == local_days{October / 26 / 2026} + 7h + 30min);
    CHECK(triggers.next == local_days{November / 2 / 2026} + 7h + 30min);
}
