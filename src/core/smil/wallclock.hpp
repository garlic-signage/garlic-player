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
#ifndef GARLIC_CORE_SMIL_WALLCLOCK_HPP
#define GARLIC_CORE_SMIL_WALLCLOCK_HPP

#include "smil/document.hpp"

#include <chrono>
#include <optional>

namespace garlic::core::smil
{
    struct WallclockTriggers
    {
        std::optional<std::chrono::local_seconds> previous; // last time ≤ now, if there was one
        std::optional<std::chrono::local_seconds> next;     // next time > now, if there is one left
    };

    /**
     * When a wallclock fires, garlic-lib: WallClock::calculateCurrentTrigger. Everything in local time, so a daily
     * 08:00 stays 08:00 after a daylight saving change. The engine needs previous for elements that should already
     * be playing, e.g. started at 08:00 with dur="1h" and the player starts at 08:20.
     *
     * Times: start, start + 1 period, … start + repeats periods; endless for repeats -1, only start for 0.
     */
    WallclockTriggers wallclockTriggers(const Wallclock &wallclock, std::chrono::local_seconds now);
}

#endif // GARLIC_CORE_SMIL_WALLCLOCK_HPP
