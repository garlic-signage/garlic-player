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
#ifndef PLAYER_UI_SUPERVISOR_DEVICE_CONTROL_HPP
#define PLAYER_UI_SUPERVISOR_DEVICE_CONTROL_HPP

#include "player_runtime/player_runtime_api.hpp"

namespace player
{
    /// What applies master volume and brightness of the configuration (DeviceSettings of the core): the supervisor.
    /// UI thread.
    class DeviceControl
    {
        public:
            virtual ~DeviceControl()                                               = default;
            virtual void deviceSettings(const garlic::core::DeviceSettings &settings) = 0;
    };
}

#endif // PLAYER_UI_SUPERVISOR_DEVICE_CONTROL_HPP
