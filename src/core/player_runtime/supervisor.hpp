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
#ifndef GARLIC_CORE_PLAYER_RUNTIME_SUPERVISOR_HPP
#define GARLIC_CORE_PLAYER_RUNTIME_SUPERVISOR_HPP

#include <string>

namespace garlic::core
{
    /**
     * What stands over the player and may do what the player may not: the launcher on Android (or the display
     * firmware, e.g. Philips), garlic-daemon on Unix, a service on Windows. garlic-lib: Launcher.
     *
     * The core decides what has to be done, the platform part of the player implements this interface and passes it
     * on; the core knows no platform. Called in the thread of the core.
     */
    class Supervisor
    {
        public:
            virtual ~Supervisor() = default;

            /// Restart the device. task_id: of the task, "DailyReboot" (schedule) or "httpd: <local time>" (REST API).
            virtual void reboot(const std::string &task_id) = 0;

            /// Install the player software downloaded by a firmwareUpdate task.
            virtual void installSoftware(const std::string &path) = 0;
    };
}

#endif // GARLIC_CORE_PLAYER_RUNTIME_SUPERVISOR_HPP
