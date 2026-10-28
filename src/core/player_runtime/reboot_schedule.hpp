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
#ifndef GARLIC_CORE_PLAYER_RUNTIME_REBOOT_SCHEDULE_HPP
#define GARLIC_CORE_PLAYER_RUNTIME_REBOOT_SCHEDULE_HPP

#include <chrono>
#include <optional>
#include <string_view>

namespace garlic::core::player_runtime
{
    /**
     * The next scheduled restart, garlic-lib: tools/reboot (Scheduler, WeekdayConverter).
     *   days  schedule/reboot_days: English weekdays separated by spaces, any case, e.g. "monday Thursday"
     *   time  schedule/reboot_time: "HH:mm:ss" or "HH:mm", 03:00:00 if missing or invalid
     * The earliest time within a week from now; today counts if the time has not passed yet. nullopt without days.
     */
    std::optional<std::chrono::local_seconds> nextReboot(std::string_view days, std::string_view time,
                                                         std::chrono::local_seconds now);
}

#endif // GARLIC_CORE_PLAYER_RUNTIME_REBOOT_SCHEDULE_HPP
