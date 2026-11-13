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

import android.Manifest;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.view.WindowManager;

/**
 * The activity of the player. Its name and package are the ones of the legacy player: launcher 1.x and the Philips
 * firmware start and update the player by them.
 *
 * Finds the launcher before Qt starts, so main() of the C++ part finds it ready, and offers it to C++ through static
 * methods (QJniObject::callStaticMethod). Without a launcher they do nothing.
 */
public class GarlicActivity extends org.qtproject.qt.android.bindings.QtActivity
{
    private static volatile LauncherInterface launcher = null;
    private static volatile GarlicActivity    instance = null;

    private final BroadcastReceiver config_receiver = new ConfigReceiver();
    private final BroadcastReceiver index_receiver  = new SmilIndexReceiver();

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        // before Qt: main() runs in a thread of its own as soon as super.onCreate() has loaded Qt
        instance = this;
        if (isPackageInstalled(GarlicLauncher.PACKAGE_NAME))
            launcher = new GarlicLauncher(getApplicationContext());
        else if (isPackageInstalled(PhilipsLauncher.PACKAGE_NAME))
            launcher = new PhilipsLauncher(getApplicationContext());
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        register(config_receiver, ConfigReceiver.ACTION);
        register(index_receiver, SmilIndexReceiver.ACTION);

        super.onCreate(savedInstanceState);

        if (!hasStoragePermission())
            requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE,
                                            Manifest.permission.WRITE_EXTERNAL_STORAGE}, 1);
    }

    @Override
    protected void onDestroy()
    {
        unregisterReceiver(config_receiver);
        unregisterReceiver(index_receiver);
        super.onDestroy();
    }

    // ------------------------------------------------------------------ for C++

    /// "garlic", "philips" or "" without launcher
    public static String launcherName()
    {
        LauncherInterface l = launcher;
        return l != null ? l.getLauncherName() : "";
    }

    public static void fetchDeviceInformation()
    {
        LauncherInterface l = launcher;
        if (l != null)
            l.fetchDeviceInformation();
    }

    public static String launcherUuid()
    {
        LauncherInterface l = launcher;
        return l != null ? l.getUUIDFromLauncher() : "";
    }

    public static String launcherContentUrl()
    {
        LauncherInterface l = launcher;
        return l != null ? l.getContentUrlFromLauncher() : "";
    }

    public static String launcherVersion()
    {
        LauncherInterface l = launcher;
        return l != null ? l.getLauncherVersion() : "";
    }

    public static void screenOff()
    {
        LauncherInterface l = launcher;
        if (l != null)
            l.setScreenOff();
    }

    public static void screenOn()
    {
        LauncherInterface l = launcher;
        if (l != null)
            l.setScreenOn();
    }

    public static void deepStandby(String seconds_to_wakeup)
    {
        LauncherInterface l = launcher;
        if (l != null)
            l.activateDeepStandBy(seconds_to_wakeup);
    }

    public static void reboot(String task_id)
    {
        LauncherInterface l = launcher;
        if (l != null)
            l.rebootOS(task_id);
    }

    public static void installSoftware(String path)
    {
        LauncherInterface l = launcher;
        if (l != null)
            l.installSoftware(path);
    }

    public static void closedCorrectly()
    {
        LauncherInterface l = launcher;
        if (l != null)
            l.closedCorrectly();
    }

    /// Cache and logs are on the shared storage like in the legacy player: the launcher installs the downloaded APK
    /// from there.
    public static boolean storagePermitted()
    {
        GarlicActivity activity = instance;
        return activity != null && activity.hasStoragePermission();
    }

    // ------------------------------------------------------------------ helpers

    private boolean hasStoragePermission()
    {
        return checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
    }

    /// Android 13+ asks for the flag; the broadcasts of the launcher come from another app.
    private void register(BroadcastReceiver receiver, String action)
    {
        IntentFilter filter = new IntentFilter(action);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
            registerReceiver(receiver, filter, Context.RECEIVER_EXPORTED);
        else
            registerReceiver(receiver, filter);
    }

    private boolean isPackageInstalled(String name)
    {
        try
        {
            getPackageManager().getPackageInfo(name, 0);
            return true;
        }
        catch (PackageManager.NameNotFoundException e)
        {
            return false;
        }
    }
}
