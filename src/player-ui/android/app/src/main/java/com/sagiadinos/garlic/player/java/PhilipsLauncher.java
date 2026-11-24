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

import java.util.Arrays;
import java.util.Calendar;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * Philips displays (TPV launcher): screen over SICP on 127.0.0.1:5000, restart and installation with the broadcasts of
 * the firmware. uuid is model number and serial, the version the firmware build; they come later over the socket.
 * As in the legacy player. Brightness of the configuration as Smart Power of SICP (not in the legacy player), see
 * setBrightness(). Deep standby (not in the legacy player): the wake time into scheduling page 7, read back, then power
 * off, see activateDeepStandBy(); with standby_mode deep the player clears all scheduling pages at the start and after
 * waking, see takeOverSchedule(). No temperature: the one of SICP is the board, the core reads the SoC.
 */
public class PhilipsLauncher implements LauncherInterface
{
    public static final String PACKAGE_NAME = "com.tpv.app.tpvlauncher";

    private static final String TAG = "PhilipsLauncher";
    private static final int    FLAG_RECEIVER_INCLUDE_BACKGROUND = 0x1000000;

    private static final byte SCHEDULE_PAGE   = 7;    // of 1…7; overwritten at every deep standby
    private static final byte SOURCE_CUSTOM   = 0x18; // the custom app: the player
    private static final long MIN_SLEEP_S     = 120;  // shorter: the page would come before the power off
    private static final byte NULL_HOUR       = 24;   // SICP: no time
    private static final byte NULL_MINUTE     = 60;

    private final Context      context;
    private final SocketClient socket;
    private volatile String serial_number    = "";
    private volatile String model_number     = "";
    private volatile String launcher_version = "";
    private volatile String display_model    = ""; // for the report: by content, the lengths differ per series

    /// The schedule commands one after the other: a clear after waking must not overwrite the next wake time
    private final ExecutorService             schedule         = Executors.newSingleThreadExecutor();
    private final LinkedBlockingQueue<byte[]> schedule_reports = new LinkedBlockingQueue<>();
    private final LinkedBlockingQueue<byte[]> model_reports    = new LinkedBlockingQueue<>(); // 0xA1, see ask()

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
            if (ret[3] == 0x5B) // a scheduling page, read back by scheduled()
                schedule_reports.offer(ret);
            else if (ret[3] == 0x15) // serial
                serial_number = SICPDef.convertBytesToString(ret);
            else if (ret[3] == (byte) 0xA1) // model number or firmware, in the order asked, see ask()
                model_reports.offer(ret);
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

    /**
     * Serial (0x15, told apart by its code), then model number and firmware (both 0xA1) one after the other: an answer
     * of 0xA1 does not say what was asked, and its length changed with SICP 2.09 (model "43BDL4550D/00" instead of
     * "43BDL4550D", firmware "02.30" without "V"), so the legacy player, which told them apart by length, took neither
     * on the BDL4550D. Waits for the answers: called in a thread of its own, again until the uuid is complete.
     */
    @Override
    public void fetchDeviceInformation()
    {
        send(SICPCommand.SICP_COMMAND_GET_SERIAL_CODE);
        String model = ask(SICPCommand.SICP_COMMAND_GET_MODEL_NUMBER);
        if (model != null)
        {
            display_model = model.trim();                 // for the report: "43BDL4550D/00"
            model_number  = display_model.split("/")[0]; // the uuid as of the legacy player: "43BDL4550D-<serial>"
        }
        String firmware = ask(SICPCommand.SICP_COMMAND_GET_FIRMWARE_BUILD);
        if (firmware != null)
            launcher_version = firmware.trim();
    }

    /**
     * Asks 0xA1 and waits up to 1 s for its answer; null without one or with NAV (0x18 as the only byte: the display
     * does not have it, SICP 2.13 3.2.1). Answers that came too late for an earlier question are dropped first.
     */
    private String ask(SICPCommand command)
    {
        model_reports.clear();
        send(command);
        byte[] report;
        try
        {
            report = model_reports.poll(1, TimeUnit.SECONDS);
        }
        catch (InterruptedException e)
        {
            Thread.currentThread().interrupt();
            return null;
        }
        if (report == null || (report.length == 6 && report[4] == 0x18))
            return null;
        return SICPDef.convertBytesToString(report);
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
        String serial = serial_number.trim();
        if (model_number.isEmpty() || serial.isEmpty())
            return "";
        return model_number + "-" + serial;
    }

    @Override
    public String getLauncherVersion()
    {
        return launcher_version;
    }

    /// Model and serial over SICP (0xA1 0x00, 0x15); the BDL4550D: "43BDL4550D/00", "AU0A2123004336"
    @Override
    public String[] getDisplayHardware()
    {
        return new String[]{display_model, "", serial_number.trim(), "Philips"};
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
     * SICP has no brightness for the player: the video parameters (0x32, chapter 5.1) do not work for an installed apk,
     * the firmware answers "not available". What it has is Smart Power (0xDD, chapter 7.2), a power saving that dims
     * the backlight in levels, the higher the darker: below 33 High, below 66 Medium, else Off. Low is the same as Off,
     * says the document. On the BDL4550D High is only a little darker.
     */
    @Override
    public void setBrightness(int percent)
    {
        byte level = percent < 33 ? (byte) 0x03 : percent < 66 ? (byte) 0x02 : (byte) 0x00;
        socket.sendSICPCommand(SICPDef.command((byte) 0xDD, level));
        Log.i(TAG, "brightness " + percent + " %: Smart Power " + level);
    }

    /**
     * The display wakes itself: the player sleeps with it and cannot send anything until then. So first the wake time
     * goes into scheduling page 7 (SICP 0x5A: on at that time with the source Custom, no off time), on its weekday and
     * every week, then power off (SICP 0x18 0x01: backlight off, CPU clock low). The time is the local one of the
     * display, rounded up to the minute, at least 2 minutes away. Before the power off the page is read back (0x5B):
     * if it does not come back as written, the display does not go off, only the backlight (on again in
     * deepStandbyEnded()). 1 s between the commands, enough on the BDL4550D. The page stays until
     * deepStandbyEnded() or the next start clears it.
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
                                      NULL_HOUR, NULL_MINUTE, // no off time
                                      SOURCE_CUSTOM, weekdays(wake));
        byte[] off  = SICPDef.command((byte) 0x18, (byte) 0x01);
        schedule.execute(() -> {
            socket.sendSICPCommand(page);
            SystemClock.sleep(1000);
            if (!scheduled(page))
            {
                Log.e(TAG, "deep standby: page " + SCHEDULE_PAGE + " not read back as written, only the backlight goes"
                           + " off");
                setScreenOff();
                return;
            }
            Log.i(TAG, String.format("deep standby until %tF %<tR", wake));
            socket.sendSICPCommand(off);
        });
    }

    /// Backlight on (only off if the wake time could not be read back) and the scheduling pages cleared
    @Override
    public void deepStandbyEnded()
    {
        setScreenOn();
        takeOverSchedule();
    }

    /**
     * standby_mode deep: only the CMS switches the display. All scheduling pages 1…7 disabled and emptied (0x5A, page
     * in the high nibble, 0 disabled, times 24:60, no source, no days), also the ones set up in the menu: the firmware
     * switches exactly at the times of a page, an old page would switch the display off or on beside the SMIL. 300 ms
     * between the pages.
     */
    @Override
    public void takeOverSchedule()
    {
        schedule.execute(() -> {
            for (byte p = 1; p <= 7; ++p)
            {
                socket.sendSICPCommand(SICPDef.command((byte) 0x5A, (byte) (p << 4), NULL_HOUR, NULL_MINUTE, NULL_HOUR,
                                                       NULL_MINUTE, (byte) 0x00, (byte) 0x00));
                SystemClock.sleep(300);
            }
            Log.i(TAG, "standby_mode deep: scheduling pages 1-7 cleared");
        });
    }

    /**
     * Reads page 7 back (0x5B) and compares it with the written command. The report: 0x5B, enabled, start hour and
     * minute, end hour and minute, source, working days, from the 4th byte on like the command after its page byte.
     * 3 s for the answer.
     */
    private boolean scheduled(byte[] page)
    {
        schedule_reports.clear();
        socket.sendSICPCommand(SICPDef.command((byte) 0x5B, SCHEDULE_PAGE));
        byte[] report;
        try
        {
            report = schedule_reports.poll(3, TimeUnit.SECONDS);
        }
        catch (InterruptedException e)
        {
            Thread.currentThread().interrupt();
            return false;
        }
        if (report == null)
        {
            Log.w(TAG, "scheduling page " + SCHEDULE_PAGE + ": no answer");
            return false;
        }
        boolean same = report.length >= 12 && report[4] == 0x01
                       && Arrays.equals(Arrays.copyOfRange(report, 5, 11), Arrays.copyOfRange(page, 5, 11));
        if (!same)
            Log.w(TAG, "scheduling page " + SCHEDULE_PAGE + " written " + hex(page) + ", read " + hex(report));
        return same;
    }

    private static String hex(byte[] bytes)
    {
        StringBuilder text = new StringBuilder();
        for (byte b : bytes)
            text.append(SICPDef.convertByteToHexString(b)).append(' ');
        return text.toString().trim();
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
