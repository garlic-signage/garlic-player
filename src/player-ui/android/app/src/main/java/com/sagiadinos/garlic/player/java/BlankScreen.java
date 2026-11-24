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
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.function.LongSupplier;

/**
 * adapi:blankScreen, like BlankScreen of the sdl player (garlic-lib: PlayerRefCommand): the activity covers the whole
 * screen black while an element runs (covers()), on every device and with every standby_mode. Switching the screen
 * off goes only through the launcher, with the standby_mode of the configuration:
 *   partially   the screen goes off until the element ends
 *   deep        the launcher sleeps until the planned end of the element and wakes the device; without a planned end
 *               the screen goes off instead
 *   no_standby  only black (also any other value)
 * Without launcher only black.
 */
final class BlankScreen
{
    private static final String TAG = "garlic-player";

    private LauncherInterface        control;
    private final Set<Long>          covering = new HashSet<>(); // instances of running elements
    private final Map<Long, Boolean> active   = new HashMap<>(); // instance → the screen was switched off

    void setControl(LauncherInterface launcher)
    {
        control = launcher;
    }

    /** The element begins; remaining (seconds, -1 unknown) is asked only for deep */
    void begin(long instance, String standby_mode, LongSupplier remaining)
    {
        covering.add(instance);
        if (!standby_mode.equals("partially") && !standby_mode.equals("deep"))
            return;
        if (control == null)
        {
            Log.i(TAG, "blankScreen: no launcher, the screen stays on, only black");
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

    /**
     * The element ends: the screen goes on again if begin() switched it off; after a deep standby the launcher is told
     * that the device is awake. Unknown instances: nothing.
     */
    void end(long instance)
    {
        covering.remove(instance);
        Boolean switched_off = active.remove(instance);
        if (switched_off == null || control == null)
            return;
        if (switched_off)
            control.setScreenOn();
        else
            control.deepStandbyEnded();
    }

    /** An element runs: black over everything but the dialogs */
    boolean covers()
    {
        return !covering.isEmpty();
    }
}
