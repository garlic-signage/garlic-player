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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/**
 * The launcher found a SMIL index, e.g. on a USB stick: extra smil_index_path. The action name is the one of
 * launcher 1.x.
 */
public class SmilIndexReceiver extends BroadcastReceiver
{
    public static final String ACTION = "com.sagiadinos.garlic.player.java.SmilIndexReceiver";

    @Override
    public void onReceive(Context context, Intent intent)
    {
        if (intent == null || !ACTION.equals(intent.getAction()))
            return;
        String path = intent.getStringExtra("smil_index_path");
        if (path != null && !path.isEmpty())
            onSmilIndexPath(path);
    }

    /// C++: plays this index from now on.
    public static native void onSmilIndexPath(String path);
}
