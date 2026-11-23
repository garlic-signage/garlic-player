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
import android.os.SystemClock;
import android.util.Log;

import com.sagiadinos.garlic.player.java.SICPDef.SICPCommand;
import com.sagiadinos.garlic.player.java.SocketClient.SocketResponse;

import java.util.Calendar;

/**
 * Philips displays (TPV launcher): screen over SICP on 127.0.0.1:5000, restart and installation with the broadcasts of
 * the firmware. uuid is model number and serial, the version the firmware build; they come later over the socket.
 * As in the legacy player. Brightness of the configuration over SICP (not in the legacy player). Deep standby (not in the legacy player): the wake time into scheduling page 7, then power
 * off, see activateDeepStandBy(). No temperature: the one of SICP is the board, the core reads the SoC.
 */
public class PhilipsLauncher implements LauncherInterface
{
    public static final String PACKAGE_NAME = "com.tpv.app.tpvlauncher";

    private static final String TAG = "PhilipsLauncher";
    private static final int    FLAG_RECEIVER_INCLUDE_BACKGROUND = 0x1000000;

    private static final byte SCHEDULE_PAGE   = 7;    // of 1…7; overwritten at every deep standby
    private static final byte SOURCE_CUSTOM   = 0x18; // the custom app: the player
    private static final long MIN_SLEEP_S     = 120;  // shorter: the page would come before the power off

    private final Context      context;
    private final SocketClient socket;
    private volatile String serial_number    = "";
    private volatile String model_number     = "";
    private volatile String launcher_version = "";
    private volatile Integer pending_brightness; // set as soon as the video parameters are reported

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
            else if (ret[3] == 0x33 && ret.length >= 12) // video parameters: brightness … gamma
                brightnessReported(ret);
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
    public void fetchThermal() {}

    @Override
    public Integer getTemperatureCpu()
    {
        return null;
    }

    @Override
    public Boolean getThrottling()
    {
        return null;
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

    /**
     * Brightness is one of the seven video parameters (SICP 0x32, chapter 5.1), which are only set together: the
     * current ones are asked (0x33) and set again with the new brightness when they come. It is the brightness of the
     * picture setting, not the backlight level (SICP has none); the document says 0x32 does not work on the QL3
     * platform for apps.
     */
    @Override
    public void setBrightness(int percent)
    {
        pending_brightness = Math.max(0, Math.min(100, percent));
        socket.sendSICPCommand(SICPDef.command((byte) 0x33));
    }

    /// The video parameters are reported: set them again with the new brightness
    private void brightnessReported(byte[] report)
    {
        Integer brightness = pending_brightness;
        if (brightness == null)
            return;
        pending_brightness = null;
        byte[] data = new byte[8];
        data[0]     = 0x32;
        System.arraycopy(report, 4, data, 1, 7); // brightness, color, contrast, sharpness, tint, black level, gamma
        data[1]     = brightness.byteValue();
        socket.sendSICPCommand(SICPDef.command(data));
        Log.i(TAG, "brightness " + brightness);
    }

    /**
     * The display wakes itself: the player sleeps with it and cannot send anything until then. So first the wake time
     * goes into scheduling page 7 (SICP 0x5A: on at that time with the source Custom, no off time), on its weekday and
     * every week, then power off (SICP 0x18 0x01: backlight off, CPU clock low). The time is the local one of the
     * display, rounded up to the minute, at least 2 minutes away. The page stays; the next deep standby overwrites it.
     * 1 s between the two commands, enough on the BDL4550D.
     */
    @Override
    public void activateDeepStandBy(String seconds_to_wakeup)
    {
        long seconds;
        try
        {
            seconds = Long.parseLong(seconds_to_wakeup.trim());
        }
        catch (NumberFormatException e)
        {
            Log.w(TAG, "deep standby without a wake time, the screen goes off instead: " + seconds_to_wakeup);
            setScreenOff();
            return;
        }
        Calendar wake = Calendar.getInstance();
        wake.add(Calendar.SECOND, (int) Math.max(seconds, MIN_SLEEP_S));
        if (wake.get(Calendar.SECOND) > 0 || wake.get(Calendar.MILLISECOND) > 0)
            wake.add(Calendar.MINUTE, 1); // not before the end of the element
        wake.set(Calendar.SECOND, 0);
        wake.set(Calendar.MILLISECOND, 0);

        byte[] page = SICPDef.command((byte) 0x5A, (byte) (SCHEDULE_PAGE << 4 | 0x01),
                                      (byte) wake.get(Calendar.HOUR_OF_DAY), (byte) wake.get(Calendar.MINUTE),
                                      (byte) 24, (byte) 60, // no off time
                                      SOURCE_CUSTOM, weekdays(wake));
        byte[] off  = SICPDef.command((byte) 0x18, (byte) 0x01);
        Log.i(TAG, String.format("deep standby until %tF %<tR", wake));
        new Thread(() -> {
            socket.sendSICPCommand(page);
            SystemClock.sleep(1000);
            socket.sendSICPCommand(off);
        }, "deep standby").start();
    }

    /// SICP working days: bit 0 every week, bit 1 Monday … bit 7 Sunday
    static byte weekdays(Calendar day)
    {
        int weekday = day.get(Calendar.DAY_OF_WEEK); // Sunday 1 … Saturday 7
        int bit     = weekday == Calendar.SUNDAY ? 7 : weekday - 1;
        return (byte) (0x01 | 1 << bit);
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
        // nothing to report: the Philips firmware starts the player again after a crash
    }

    private void send(SICPCommand command)
    {
        socket.sendSICPCommand(SICPDef.getSICPCommand(command));
    }
}
