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

#include "fake_clock.hpp"
#include "common/clock.hpp"

using garlic::core::common::SystemClock;
using garlic::core::tests::FakeClock;
using namespace std::chrono_literals;

TEST_CASE("SystemClock returns the time of the operating system")
{
    SystemClock clock;

    auto difference = std::chrono::system_clock::now() - clock.now();
    CHECK(difference < 5s);
    CHECK(difference > -5s);

    auto first = clock.monotonic();
    CHECK(clock.monotonic() >= first);
}

TEST_CASE("FakeClock moves only on advance()")
{
    FakeClock clock;
    auto      wall   = clock.now();
    auto      steady = clock.monotonic();

    CHECK(clock.now() == wall);

    clock.advance(30s);
    CHECK(clock.now() == wall + 30s);
    CHECK(clock.monotonic() == steady + 30s);
}

TEST_CASE("FakeClock: changing the wall time does not affect the monotonic time")
{
    FakeClock clock;
    auto      steady = clock.monotonic();

    clock.setWallTime(std::chrono::sys_days{std::chrono::March / 1 / 2020});
    CHECK(clock.now() == std::chrono::sys_days{std::chrono::March / 1 / 2020});
    CHECK(clock.monotonic() == steady);
}
