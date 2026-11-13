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
#ifndef PLAYER_UI_SUPERVISOR_SCREEN_CONTROL_HPP
#define PLAYER_UI_SUPERVISOR_SCREEN_CONTROL_HPP

#include <chrono>

namespace player
{
    /// What can switch the screen off at adapi:blankScreen: the supervisor, the Android launcher. UI thread.
    class ScreenControl
    {
        public:
            virtual ~ScreenControl()                               = default;
            virtual void screenOff()                               = 0;
            virtual void screenOn()                                = 0;
            virtual void deepStandby(std::chrono::seconds wake_up) = 0;
    };
}

#endif // PLAYER_UI_SUPERVISOR_SCREEN_CONTROL_HPP
