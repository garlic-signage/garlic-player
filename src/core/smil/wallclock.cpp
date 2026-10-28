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
#include "smil/wallclock.hpp"

#include <algorithm>
#include <cstdint>

namespace garlic::core::smil
{
    namespace
    {
        using namespace std::chrono;

        /// Like QDateTime::addMonths: the day is clamped to the end of the month, 31.01. + 1 month = 28.02.
        local_seconds addMonths(local_seconds time, std::int64_t count)
        {
            if (count == 0)
                return time;
            local_days     day  = floor<days>(time);
            year_month_day date{day};
            year_month     target = date.year() / date.month() + months(count);
            auto           end    = (target / std::chrono::last).day();
            auto           moved  = static_cast<local_days>(target / std::min(date.day(), end));
            return moved + (time - day);
        }

        /// start + count periods. Every time is calculated from start, so clamped days do not add up.
        local_seconds occurrence(const Wallclock &wallclock, std::int64_t count)
        {
            const Period &p    = wallclock.period;
            local_seconds time = addMonths(wallclock.start, (p.years * 12LL + p.months) * count);
            return time + days(p.days * count) + p.time * count;
        }

        /// Rough length of a period, only to start the search near the result.
        seconds approximateLength(const Period &p)
        {
            return duration_cast<seconds>(years(p.years) + months(p.months)) + days(p.days) + p.time;
        }
    }

    WallclockTriggers wallclockTriggers(const Wallclock &wallclock, local_seconds now)
    {
        if (now < wallclock.start)
            return {std::nullopt, wallclock.start};

        bool         endless = wallclock.repeats < 0;
        std::int64_t last    = endless ? INT64_MAX : wallclock.repeats; // index of the last time
        seconds      length  = approximateLength(wallclock.period);
        if (length <= seconds(0))
            last = 0;

        // largest count with occurrence(count) <= now: estimate, then correct
        std::int64_t count = 0;
        if (last > 0)
        {
            count = std::clamp<std::int64_t>((now - wallclock.start) / length - 1, 0, last);
            while (count > 0 && occurrence(wallclock, count) > now)
                --count;
            while (count < last && occurrence(wallclock, count + 1) <= now)
                ++count;
        }

        WallclockTriggers triggers;
        triggers.previous = occurrence(wallclock, count);
        if (count < last)
            triggers.next = occurrence(wallclock, count + 1);
        return triggers;
    }
}
