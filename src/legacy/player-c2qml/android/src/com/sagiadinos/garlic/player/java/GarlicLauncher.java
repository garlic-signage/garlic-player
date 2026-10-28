/*
 garlic-player: SMIL Player for Digital Signage
 Copyright (C) 2021 Nikolaos Saghiadinos <ns@smil-control.com>
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
import android.content.Intent;
import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import java.util.concurrent.ExecutionException;


public class GarlicLauncher implements LauncherInterface
{
    private AsyncTask<Void, Void, String> runningTaskSmilIndex;
    private AsyncTask<Void, Void, String> runningTaskUUID;
    private AsyncTask<Void, Void, String> runningTaskLauncherVersion;
    final String PROVIDER_NAME = "com.sagiadinos.garlic.launcher.SettingsProvider";

    Context MyContext;
    private String content_url      = null;
    private String uuid             = null;
    private String launcher_version = null;

    public GarlicLauncher(Context c)
    {
        MyContext = c;
    }

    public void fetchDeviceInformation()
    {
        retrieveSmilIndex();
        retrieveUUID();
        retrieveLauncherVersion();
    }

    public String getContentUrlFromLauncher()
    {
        return content_url;
    }

    public String getUUIDFromLauncher()
    {
        return uuid;
    }

    public String getLauncherVersion()
    {
        return launcher_version;
    }

    public String getLauncherName()
    {
        return "garlic";
    }

    public void setScreenOff()
    {
        Intent intent = new Intent("com.sagiadinos.garlic.launcher.receiver.CommandReceiver");
        intent.putExtra("command", "screen_off");
        MyContext.sendBroadcast(intent);
    }

    public void setScreenOn()
    {
        Intent intent = new Intent("com.sagiadinos.garlic.launcher.receiver.CommandReceiver");
        intent.putExtra("command", "screen_on");
        MyContext.sendBroadcast(intent);
    }

    public void activateDeepStandBy(String seconds_to_wakeup)
    {
        Intent intent = new Intent("com.sagiadinos.garlic.launcher.receiver.CommandReceiver");
        intent.putExtra("command", "deep_standby");
        intent.putExtra("seconds_to_wakeup", seconds_to_wakeup);
        MyContext.sendBroadcast(intent);
    }

    public void rebootOS(String task_id)
    {
        Intent intent = new Intent("com.sagiadinos.garlic.launcher.receiver.CommandReceiver");
        intent.putExtra("command", "reboot");
        intent.putExtra("task_id", task_id);
        MyContext.sendBroadcast(intent);
    }

    public void installSoftware(String apk_path)
    {
        Intent intent = new Intent("com.sagiadinos.garlic.launcher.receiver.InstallAppReceiver");
        intent.putExtra("apk_path", apk_path);
        MyContext.sendBroadcast(intent);
    }

    private void retrieveSmilIndex()
    {
        content_url = askProvider("smil_content_url");
    }

    private void retrieveUUID()
    {
        uuid = askProvider("uuid");
    }

    private void retrieveLauncherVersion()
    {
        launcher_version = askProvider("launcher_version");
    }

    private String askProvider(String appendable_path)
    {
        ContentResolver contentResolver = MyContext.getContentResolver();
        Uri BASE_URI = Uri.parse("content://" + PROVIDER_NAME);
        String value = "";

        Uri uri = BASE_URI.buildUpon().appendPath(appendable_path).build();

        Cursor cursor = contentResolver.query(uri, null, null, null, null);

        if (cursor.moveToFirst())
        {
            value = cursor.getString(0);
        }
        cursor.close();

        return value;
    }

}
