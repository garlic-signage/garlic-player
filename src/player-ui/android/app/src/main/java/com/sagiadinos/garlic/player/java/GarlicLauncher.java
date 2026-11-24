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
 *
 * Temperature and throttling from the keys temperature_cpu ("47") and throttling ("true"/"false") of the
 * ContentProvider, when the launcher has them; it can read them as device owner (HardwarePropertiesManager).
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
    private volatile Integer temperature;
    private volatile Boolean throttling;
    private volatile boolean thermal_unknown; // the launcher has neither key: not asked again
    private volatile boolean thermal_asking;

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
    public String[] getDisplayHardware()
    {
        return null; // the box does not know the screen
    }

    /// In a thread: the dialog asks every second from the UI thread
    @Override
    public void fetchThermal()
    {
        if (thermal_unknown || thermal_asking)
            return;
        thermal_asking = true;
        new Thread(() -> {
            String cpu      = askProvider("temperature_cpu");
            String throttle = askProvider("throttling");
            try
            {
                temperature = cpu.isEmpty() ? null : Integer.valueOf(cpu.trim());
            }
            catch (NumberFormatException e)
            {
                temperature = null;
            }
            throttling      = throttle.isEmpty() ? null : Boolean.valueOf(throttle.trim());
            thermal_unknown = cpu.isEmpty() && throttle.isEmpty();
            thermal_asking  = false;
        }, "launcher thermal").start();
    }

    @Override
    public Integer getTemperatureCpu()
    {
        return temperature;
    }

    @Override
    public Boolean getThrottling()
    {
        return throttling;
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
    public Integer fetchOperatingHours()
    {
        return null; // launcher 1.x does not know the screen
    }

    /**
     * temperature_cpu of the ContentProvider, the same as getTemperatureCpu(), for the core: there only where it reads
     * none itself (the launcher reads it as device owner, the player usually cannot). The information dialog shows the
     * one of the launcher first, as before. Asked directly: the caller is a thread of its own.
     */
    @Override
    public Integer fetchTemperatureCpuFallback()
    {
        if (thermal_unknown)
            return null;
        String cpu = askProvider("temperature_cpu");
        try
        {
            temperature = cpu.isEmpty() ? null : Integer.valueOf(cpu.trim());
        }
        catch (NumberFormatException e)
        {
            temperature = null;
        }
        return temperature;
    }

    @Override
    public void deepStandbyEnded()
    {
        // launcher 1.x wakes the device itself
    }

    @Override
    public void takeOverSchedule()
    {
        // launcher 1.x has no schedule of its own
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

    /// launcher 1.x has no command for it: it stores the brightness of a config.xml, but never applies it
    @Override
    public void setBrightness(int percent) {}

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
