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
package com.sagiadinos.garlic.player.java;

import android.util.Log;

import java.util.HashMap;
import java.util.Map;
import java.util.function.LongSupplier;

/**
 * adapi:blankScreen with the standby_mode of the configuration, like BlankScreen of the other players
 * (supervisor/blank_screen.cpp; garlic-lib: PlayerRefCommand):
 *   partially   the screen goes off until the element ends
 *   deep        the launcher sleeps until the planned end of the element and wakes the device; without a planned end
 *               the screen goes off instead
 *   no_standby  nothing (also any other value)
 * Without launcher the screen stays on.
 */
final class BlankScreen
{
    private static final String TAG = "garlic-player";

    private LauncherInterface        control;
    private final Map<Long, Boolean> active = new HashMap<>(); // instance → the screen was switched off

    void setControl(LauncherInterface launcher)
    {
        control = launcher;
    }

    /** The element begins; remaining (seconds, -1 unknown) is asked only for deep */
    void begin(long instance, String standby_mode, LongSupplier remaining)
    {
        if (!standby_mode.equals("partially") && !standby_mode.equals("deep"))
            return;
        if (control == null)
        {
            Log.i(TAG, "blankScreen: no launcher, the screen stays on");
            return;
        }
        if (standby_mode.equals("deep"))
        {
            long rest = remaining.getAsLong();
            if (rest >= 0)
            {
                control.activateDeepStandBy(String.valueOf(rest)); // launcher 1.x takes the seconds as string
                active.put(instance, false);                       // the launcher wakes the device
                return;
            }
            Log.w(TAG, "blankScreen: deep standby without a planned end, the screen goes off instead");
        }
        control.setScreenOff();
        active.put(instance, true);
    }

    /** The element ends: the screen goes on again if begin() switched it off. Unknown instances: nothing. */
    void end(long instance)
    {
        Boolean switched_off = active.remove(instance);
        if (switched_off != null && switched_off && control != null)
            control.setScreenOn();
    }
}
