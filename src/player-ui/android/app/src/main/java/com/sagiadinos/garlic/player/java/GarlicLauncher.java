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

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.util.Log;

/**
 * garlic-launcher 1.x: commands as broadcasts, device information from its ContentProvider. The broadcasts give no
 * answer. The launcher is frozen at 1.x; the names of actions and extras must stay as they are.
 *
 * Unlike the legacy player the broadcasts name the package of the launcher: its CommandReceiver and
 * InstallAppReceiver are declared in its manifest, and Android 8+ does not deliver implicit broadcasts to them.
 */
public class GarlicLauncher implements LauncherInterface
{
    public static final String PACKAGE_NAME = "com.sagiadinos.garlic.launcher";

    private static final String TAG           = "GarlicLauncher";
    private static final String PROVIDER_NAME = PACKAGE_NAME + ".SettingsProvider";
    private static final String RECEIVER      = PACKAGE_NAME + ".receiver.";

    private final Context context;
    private String content_url      = "";
    private String uuid             = "";
    private String launcher_version = "";

    public GarlicLauncher(Context c)
    {
        context = c;
    }

    @Override
    public String getLauncherName()
    {
        return "garlic";
    }

    @Override
    public void fetchDeviceInformation()
    {
        content_url      = askProvider("smil_content_url");
        uuid             = askProvider("uuid");
        launcher_version = askProvider("launcher_version");
    }

    @Override
    public String getContentUrlFromLauncher()
    {
        return content_url;
    }

    @Override
    public String getUUIDFromLauncher()
    {
        return uuid;
    }

    @Override
    public String getLauncherVersion()
    {
        return launcher_version;
    }

    @Override
    public void setScreenOff()
    {
        sendCommand(command("screen_off"));
    }

    @Override
    public void setScreenOn()
    {
        sendCommand(command("screen_on"));
    }

    @Override
    public void activateDeepStandBy(String seconds_to_wakeup)
    {
        Intent intent = command("deep_standby");
        intent.putExtra("seconds_to_wakeup", seconds_to_wakeup);
        sendCommand(intent);
    }

    @Override
    public void rebootOS(String task_id)
    {
        Intent intent = command("reboot");
        intent.putExtra("task_id", task_id);
        sendCommand(intent);
    }

    @Override
    public void installSoftware(String apk_path)
    {
        Intent intent = new Intent(RECEIVER + "InstallAppReceiver");
        intent.putExtra("apk_path", apk_path);
        sendCommand(intent);
    }

    @Override
    public void closedCorrectly()
    {
        Intent intent = new Intent(RECEIVER + "PlayerClosedReceiver");
        intent.addFlags(Intent.FLAG_INCLUDE_STOPPED_PACKAGES);
        sendCommand(intent);
    }

    private Intent command(String name)
    {
        Intent intent = new Intent(RECEIVER + "CommandReceiver");
        intent.putExtra("command", name);
        return intent;
    }

    private void sendCommand(Intent intent)
    {
        intent.setPackage(PACKAGE_NAME);
        context.sendBroadcast(intent);
    }

    /// content://com.sagiadinos.garlic.launcher.SettingsProvider/<key>, the first column of the first row
    private String askProvider(String key)
    {
        ContentResolver resolver = context.getContentResolver();
        Uri uri = Uri.parse("content://" + PROVIDER_NAME).buildUpon().appendPath(key).build();
        try (Cursor cursor = resolver.query(uri, null, null, null, null))
        {
            if (cursor != null && cursor.moveToFirst())
            {
                String value = cursor.getString(0);
                return value != null ? value : "";
            }
        }
        catch (RuntimeException e) // no provider, no permission, an older launcher without the key
        {
            Log.w(TAG, "SettingsProvider " + key + ": " + e.getMessage());
        }
        return "";
    }
}
