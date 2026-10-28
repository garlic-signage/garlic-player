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
#ifndef GARLIC_CORE_COMMON_CLOCK_HPP
#define GARLIC_CORE_COMMON_CLOCK_HPP

#include <chrono>

namespace garlic::core::common
{
    /**
     * Source of time for the core. Injected everywhere instead of calling std::chrono directly, so tests can use a
     * simulated clock.
     */
    class Clock
    {
        public:
            virtual ~Clock() = default;

            /// Wall-clock time (UTC): HTTP dates, logs, SMIL timings, adapi-date().
            virtual std::chrono::system_clock::time_point now() const noexcept = 0;

            /// Monotonic time for timeouts and durations. Not affected when the system time is changed.
            virtual std::chrono::steady_clock::time_point monotonic() const noexcept = 0;
    };

    /// Clock of the operating system.
    class SystemClock final : public Clock
    {
        public:
            std::chrono::system_clock::time_point now() const noexcept override;
            std::chrono::steady_clock::time_point monotonic() const noexcept override;
    };
}

#endif // GARLIC_CORE_COMMON_CLOCK_HPP
