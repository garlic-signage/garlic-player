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
#ifndef GARLIC_CORE_TESTS_FAKE_CLOCK_HPP
#define GARLIC_CORE_TESTS_FAKE_CLOCK_HPP

#include "garlic/core/common/clock.hpp"

namespace garlic::core::tests
{
    /// Clock that only moves when the test calls advance(). Starts at 2026-01-01 00:00:00 UTC.
    class FakeClock final : public common::Clock
    {
        public:
            std::chrono::system_clock::time_point now() const noexcept override { return wall; }
            std::chrono::steady_clock::time_point monotonic() const noexcept override { return steady; }

            void advance(std::chrono::steady_clock::duration duration) noexcept
            {
                wall += std::chrono::duration_cast<std::chrono::system_clock::duration>(duration);
                steady += duration;
            }

            /// Changes only the wall-clock time, like a user or NTP setting the system time.
            void setWallTime(std::chrono::system_clock::time_point time) noexcept { wall = time; }

        private:
            std::chrono::system_clock::time_point wall = std::chrono::sys_days{std::chrono::January / 1 / 2026};
            std::chrono::steady_clock::time_point steady{};
    };
}

#endif // GARLIC_CORE_TESTS_FAKE_CLOCK_HPP
