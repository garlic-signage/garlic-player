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
#include "blank_screen.hpp"

#include "configuration/configuration.hpp"

#include <QDebug>

namespace player
{
    namespace cfg = garlic::core::configuration;

    void BlankScreen::begin(int instance, std::string_view standby_mode, const Remaining &remaining)
    {
        if (standby_mode != cfg::standby_partially && standby_mode != cfg::standby_deep)
            return;
        if (control == nullptr)
        {
            qInfo() << "blankScreen: no supervisor, the screen stays on";
            return;
        }
        if (standby_mode == cfg::standby_deep)
        {
            if (auto rest = remaining ? remaining() : std::nullopt)
            {
                control->deepStandby(*rest);
                active[instance] = false; // the supervisor wakes the device
                return;
            }
            qWarning() << "blankScreen: deep standby without a planned end, the screen goes off instead";
        }
        control->screenOff();
        active[instance] = true;
    }

    void BlankScreen::end(int instance)
    {
        auto it = active.find(instance);
        if (it == active.end())
            return;
        if (it->second && control != nullptr)
            control->screenOn();
        active.erase(it);
    }
}
