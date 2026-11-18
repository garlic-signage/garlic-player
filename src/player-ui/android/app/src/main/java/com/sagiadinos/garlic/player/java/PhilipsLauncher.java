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

import android.content.Context;
import android.content.Intent;
import android.util.Log;

import com.sagiadinos.garlic.player.java.SICPDef.SICPCommand;
import com.sagiadinos.garlic.player.java.SocketClient.SocketResponse;

/**
 * Philips displays (TPV launcher): screen over SICP on 127.0.0.1:5000, restart and installation with the broadcasts of
 * the firmware. uuid is model number and serial, the version the firmware build; they come later over the socket.
 * As in the legacy player; no deep standby.
 */
public class PhilipsLauncher implements LauncherInterface
{
    public static final String PACKAGE_NAME = "com.tpv.app.tpvlauncher";

    private static final String TAG = "PhilipsLauncher";
    private static final int    FLAG_RECEIVER_INCLUDE_BACKGROUND = 0x1000000;

    private final Context      context;
    private final SocketClient socket;
    private volatile String serial_number    = "";
    private volatile String model_number     = "";
    private volatile String launcher_version = "";

    private final SocketResponse response = new SocketResponse()
    {
        @Override
        public void sockResponse(byte[] ret)
        {
            if (ret == null || ret.length < 4 || ret[ret.length - 1] != SICPDef.getDataCheckSum(ret))
            {
                Log.w(TAG, "SICP answer with wrong checksum");
                return;
            }
            if (ret[3] == 0x15) // serial
                serial_number = SICPDef.convertBytesToString(ret);
            else if (ret[3] == (byte) 0xA1 && ret[0] == 12) // firmware: 7 bytes + 5
                launcher_version = SICPDef.convertBytesToString(ret);
            else if (ret[3] == (byte) 0xA1 && ret[0] == 15) // model number: 10 bytes + 5
                model_number = SICPDef.convertBytesToString(ret);
            else
                Log.d(TAG, "unknown SICP answer: " + SICPDef.convertBytesToString(ret));
        }
    };

    public PhilipsLauncher(Context c)
    {
        context = c;
        socket  = new SocketClient(response);
        socket.start();
    }

    @Override
    public String getLauncherName()
    {
        return "philips";
    }

    @Override
    public void fetchDeviceInformation()
    {
        send(SICPCommand.SICP_COMMAND_GET_SERIAL_CODE);
        send(SICPCommand.SICP_COMMAND_GET_MODEL_NUMBER);
        send(SICPCommand.SICP_COMMAND_GET_FIRMWARE_BUILD);
    }

    /// The legacy player took this as content URL on Philips; the new one only as default without one.
    @Override
    public String getContentUrlFromLauncher()
    {
        return "https://indexes.smil-control.com";
    }

    @Override
    public String getUUIDFromLauncher()
    {
        if (model_number.isEmpty() || serial_number.isEmpty())
            return "";
        return model_number + "-" + serial_number;
    }

    @Override
    public String getLauncherVersion()
    {
        return launcher_version;
    }

    @Override
    public void setScreenOff()
    {
        send(SICPCommand.SICP_COMMAND_SET_BACKLIGHT_DISABLE);
    }

    @Override
    public void setScreenOn()
    {
        send(SICPCommand.SICP_COMMAND_SET_BACKLIGHT_ENABLE);
    }

    @Override
    public void activateDeepStandBy(String seconds_to_wakeup)
    {
        Log.w(TAG, "no deep standby on Philips");
    }

    @Override
    public void rebootOS(String task_id)
    {
        Intent intent = new Intent("php.intent.action.REBOOT");
        intent.addFlags(FLAG_RECEIVER_INCLUDE_BACKGROUND);
        context.sendBroadcast(intent);
    }

    @Override
    public void installSoftware(String apk_path)
    {
        Intent intent = new Intent("php.intent.action.UPDATE_APK");
        intent.putExtra("filePath", apk_path);
        intent.putExtra("keep", false);
        intent.putExtra("packageName", context.getPackageName());
        intent.putExtra("activityName", GarlicActivity.class.getName());
        intent.putExtra("isAllowDowngrade", true);
        intent.addFlags(FLAG_RECEIVER_INCLUDE_BACKGROUND);
        context.sendBroadcast(intent);
    }

    @Override
    public void closedCorrectly()
    {
        // nobody restarts the player on Philips
    }

    private void send(SICPCommand command)
    {
        socket.sendSICPCommand(SICPDef.getSICPCommand(command));
    }
}
