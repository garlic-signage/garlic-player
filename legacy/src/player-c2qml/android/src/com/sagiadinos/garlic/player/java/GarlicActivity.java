/*
 garlic-player: SMIL Player for Digital Signage
 Copyright (C) 2016 Nikolaos Saghiadinos <ns@smil-control.com>
 This file is part of the garlic-player source code

 This program is free software: you can redistribute it and/or  modify
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

import android.content.Context;
import android.os.Environment;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.PowerManager;
import java.util.concurrent.ExecutionException;


public class GarlicActivity extends org.qtproject.qt5.android.bindings.QtActivity
{
    private static GarlicActivity m_instance;
    private boolean is_launcher     = false;
    private static LauncherInterface MyLauncherInterface = null;

    public GarlicActivity()
    {
        m_instance = this;
    }

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        if (isGarlicLauncherInstalled())
        {
            is_launcher = true;
            MyLauncherInterface = new GarlicLauncher(this);
        }
        else if (isPhilipsLauncherInstalled())
        {
            is_launcher = true;
            MyLauncherInterface = new PhilipsLauncher(this);
        }
    }

     public void registerBroadcastReceiver()
    {
        IntentFilter filter = new IntentFilter("com.sagiadinos.garlic.player.java.ConfigReceiver");
        ConfigReceiver MyConfigReceiver = new ConfigReceiver();
        registerReceiver(MyConfigReceiver, filter);

        // needed when USB Stick enters with local SMIL File
        IntentFilter filter2 = new IntentFilter("com.sagiadinos.garlic.player.java.SmilIndexReceiver");
        SmilIndexReceiver MySmilIndexReceiver = new SmilIndexReceiver();
        registerReceiver(MySmilIndexReceiver, filter2);
    }


    public void fetchDeviceInformation()
    {
        MyLauncherInterface.fetchDeviceInformation();
    }

    public boolean isLauncherInstalled()
    {
        return is_launcher;
    }

    public String getContentUrlFromLauncher()
    {
        return MyLauncherInterface.getContentUrlFromLauncher();
    }

    public String getUUIDFromLauncher()
    {
        return MyLauncherInterface.getUUIDFromLauncher();
    }

    public String getLauncherVersion()
    {
        return MyLauncherInterface.getLauncherVersion();
    }

    public String getLauncherName()
    {
        return MyLauncherInterface.getLauncherName();
    }

    public static void setScreenOff()
    {
        MyLauncherInterface.setScreenOff();
    }

    public static void setScreenOn()
    {
        MyLauncherInterface.setScreenOn();
    }

    public static void activateDeepStandBy(String seconds_to_wakeup)
    {
        MyLauncherInterface.activateDeepStandBy(seconds_to_wakeup);
    }

    public static void rebootOS(String task_id)
    {
        MyLauncherInterface.rebootOS(task_id);
    }

    public static void installSoftware(String apk_path)
    {
        MyLauncherInterface.installSoftware(apk_path);
    }

    public static void closePlayerCorrect()
    {
         Intent intent = new Intent("com.sagiadinos.garlic.launcher.receiver.PlayerClosedReceiver");
         intent.addFlags(Intent.FLAG_INCLUDE_STOPPED_PACKAGES);
         m_instance.sendBroadcast(intent);
    }

    public static void applyConfig(String config_path)
    {
        Intent intent = new Intent("com.sagiadinos.garlic.launcher.receiver.ConfigXMLReceiver");
        intent.putExtra("config_path", config_path);
        m_instance.sendBroadcast(intent);
    }

    public static void startSecondApp(String package_name)
    {
         Intent intent = new Intent("com.sagiadinos.garlic.launcher.receiver.SecondAppReceiver");
         intent.putExtra("package_name", package_name);
         m_instance.sendBroadcast(intent);
    }

    private boolean isGarlicLauncherInstalled()
    {
        return isPackageInstalled("com.sagiadinos.garlic.launcher");
    }

    private boolean isPhilipsLauncherInstalled()
    {
        return isPackageInstalled("com.tpv.app.tpvlauncher");
    }

    private boolean isPackageInstalled(String targetPackage)
    {
        PackageManager pm = m_instance.getPackageManager();
        try
        {
            pm.getPackageInfo(targetPackage, PackageManager.GET_META_DATA);
        }
        catch (PackageManager.NameNotFoundException e)
        {
            return false;
        }
        return true;
    }

}
