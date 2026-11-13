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
#ifndef PLAYER_UI_SUPERVISOR_BLANK_SCREEN_HPP
#define PLAYER_UI_SUPERVISOR_BLANK_SCREEN_HPP

#include "screen_control.hpp"

#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <string_view>

namespace player
{
    /**
     * adapi:blankScreen with the standby_mode of the configuration, garlic-lib: PlayerRefCommand. The same for every
     * frontend:
     *   partially   the screen goes off until the element ends
     *   deep        the supervisor sleeps until the planned end of the element and wakes the device; without a
     *               planned end the screen goes off instead
     *   no_standby  nothing (also any other value)
     * Without ScreenControl the screen stays on.
     */
    class BlankScreen
    {
        public:
            using Remaining = std::function<std::optional<std::chrono::seconds>()>;

            void setControl(ScreenControl *screen) { control = screen; }

            /// The element begins; remaining is asked only for deep (PlayerRuntimeApi::remainingTime).
            void begin(int instance, std::string_view standby_mode, const Remaining &remaining);

            /// The element ends: the screen goes on again if begin() switched it off. Unknown instances: nothing.
            void end(int instance);

        private:
            ScreenControl      *control = nullptr;
            std::map<int, bool> active; // instance → the screen was switched off
    };
}

#endif // PLAYER_UI_SUPERVISOR_BLANK_SCREEN_HPP
