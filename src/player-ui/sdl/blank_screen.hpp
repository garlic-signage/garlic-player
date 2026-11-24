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
#ifndef PLAYER_UI_SDL_BLANK_SCREEN_HPP
#define PLAYER_UI_SDL_BLANK_SCREEN_HPP

#include "player_runtime/player_runtime_api.hpp"

#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string_view>

namespace player
{
    /// What can switch the screen off at adapi:blankScreen: the supervisor. UI thread.
    class ScreenControl
    {
        public:
            virtual ~ScreenControl()                               = default;
            virtual void screenOff()                               = 0;
            virtual void screenOn()                                = 0;
            virtual void deepStandby(std::chrono::seconds wake_up) = 0;
    };

    /// What applies master volume and brightness of the configuration: the supervisor. UI thread.
    class DeviceControl
    {
        public:
            virtual ~DeviceControl()                                                  = default;
            virtual void deviceSettings(const garlic::core::DeviceSettings &settings) = 0;
    };

    /**
     * adapi:blankScreen: the window covers the whole screen black while an element runs (covers()), on every device
     * and with every standby_mode. Switching the screen off goes only through the supervisor, with the standby_mode
     * of the configuration:
     *   partially   the screen goes off until the element ends
     *   deep        the supervisor sleeps until the planned end of the element and wakes the device; without a
     *               planned end the screen goes off instead
     *   no_standby  only black (also any other value)
     * Without ScreenControl only black.
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

            /// An element runs: the window draws black over everything but menu and dialogs
            bool covers() const { return !covering.empty(); }

        private:
            ScreenControl      *control = nullptr;
            std::set<int>       covering; // instances of running elements
            std::map<int, bool> active;   // instance → the screen was switched off
    };
}

#endif // PLAYER_UI_SDL_BLANK_SCREEN_HPP
