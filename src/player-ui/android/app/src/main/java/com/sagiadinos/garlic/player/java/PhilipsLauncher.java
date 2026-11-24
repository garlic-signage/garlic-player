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
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Philips displays (TPV launcher): screen over SICP on 127.0.0.1:5000, restart and installation with the broadcasts of
 * the firmware. uuid is model number and serial, the version the firmware build; they come later over the socket.
 * As in the legacy player. Brightness of the configuration as Smart Power of SICP (not in the legacy player), see
 * setBrightness(). Deep standby (not in the legacy player): the wake time into scheduling page 7, read back, then power
 * off, see activateDeepStandBy(); with standby_mode deep the player clears all scheduling pages at the start and after
 * waking, see takeOverSchedule(). No temperature: the one of SICP is the board, the core reads the SoC.
 *
 * Every command goes through one thread and waits for its answer before the next one (SICP 2.13 chapter 2.2: a new
 * command not before the previous one is acknowledged): a set command gets ACK, NACK (checksum, parameter or length
 * wrong) or NAV (correct, but this display does not have it), a get command its report or NACK/NAV; see set(), get().
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
    private static final long ANSWER_MS       = 1000; // SICP: a retry after 500 ms may be triggered; no retries here

    private enum Reply { ACK, NACK, NAV, NONE }

    private final Context      context;
    private final SocketClient socket;
    private volatile String serial_number    = "";
    private volatile String model_number     = "";
    private volatile String launcher_version = "";
    private volatile String display_model    = ""; // for the report: "43BDL4550D/00"
    private volatile int    sicp_version     = 0;  // of the firmware (0xA2 0x00), 213 for "2.13"; 0 unknown

    /// All commands one after the other (also: a clear after waking must not overwrite the next wake time)
    private final ExecutorService             sicp    = Executors.newSingleThreadExecutor();
    private final LinkedBlockingQueue<byte[]> answers = new LinkedBlockingQueue<>(); // read only in sicp

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
            answers.offer(ret); // to the command that waits for it, see exchange()
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
     * Serial (0x15), model number and firmware (both 0xA1) and once the SICP version (0xA2 0x00), one after the other:
     * an answer of 0xA1 does not say what was asked, and its length changed with SICP 2.09 (model "43BDL4550D/00"
     * instead of "43BDL4550D", firmware "02.30" without "V"), so the legacy player, which told them apart by length,
     * took neither on the BDL4550D. Waits for the answers: called in a thread of its own, again until the uuid is
     * complete; without the serial (socket not connected yet) it stops at once.
     */
    @Override
    public void fetchDeviceInformation()
    {
        await(() -> {
            String serial = text(get(SICPCommand.SICP_COMMAND_GET_SERIAL_CODE));
            if (serial == null)
                return;
            serial_number = serial;
            String model = text(get(SICPCommand.SICP_COMMAND_GET_MODEL_NUMBER));
            if (model != null)
            {
                display_model = model.trim();                 // for the report: "43BDL4550D/00"
                model_number  = display_model.split("/")[0]; // the uuid as of the legacy player: "43BDL4550D-<serial>"
            }
            String firmware = text(get(SICPCommand.SICP_COMMAND_GET_FIRMWARE_BUILD));
            if (firmware != null)
                launcher_version = firmware.trim();
            if (sicp_version == 0)
            {
                String version = text(get(SICPDef.command((byte) 0xA2, (byte) 0x00)));
                sicp_version   = version(version);
                Log.i(TAG, "SICP version " + version);
            }
        });
    }

    /// "2.13", "V2.03" → 213, 203; 0 if there is none
    static int version(String text)
    {
        if (text == null)
            return 0;
        Matcher m = Pattern.compile("(\\d+)\\.(\\d+)").matcher(text);
        return m.find() ? Integer.parseInt(m.group(1)) * 100 + Integer.parseInt(m.group(2)) : 0;
    }

    /// The characters of a report (DATA[1] on), null without one; an 0xA1 with 0x18 as its only byte is NAV (3.2.1)
    private static String text(byte[] report)
    {
        if (report == null || (report.length == 6 && report[3] == (byte) 0xA1 && report[4] == 0x18))
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

    /// SICP Operating Hours (0x0F 0x02, chapter 3.3): DATA[1] and DATA[2] are the high and low byte
    @Override
    public Integer fetchOperatingHours()
    {
        Integer[] hours = {null};
        await(() -> {
            byte[] report = get(SICPDef.command((byte) 0x0F, (byte) 0x02));
            if (report != null && report[3] == 0x0F && report.length >= 7)
                hours[0] = (report[4] & 0xFF) << 8 | (report[5] & 0xFF);
        });
        return hours[0];
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
        sicp.execute(() -> set(SICPDef.getSICPCommand(SICPCommand.SICP_COMMAND_SET_BACKLIGHT_DISABLE)));
    }

    @Override
    public void setScreenOn()
    {
        sicp.execute(() -> set(SICPDef.getSICPCommand(SICPCommand.SICP_COMMAND_SET_BACKLIGHT_ENABLE)));
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
        sicp.execute(() -> {
            if (set(SICPDef.command((byte) 0xDD, level)) == Reply.ACK)
                Log.i(TAG, "brightness " + percent + " %: Smart Power " + level);
        });
    }

    /**
     * The display wakes itself: the player sleeps with it and cannot send anything until then. So first the wake time
     * goes into scheduling page 7 (SICP 0x5A: on at that time with the source Custom, no off time), on its weekday and
     * every week, then power off (SICP 0x18 0x01: backlight off, CPU clock low). The time is the local one of the
     * display, rounded up to the minute, at least 2 minutes away. The page must be acknowledged and read back (0x5B) as
     * written, else the display does not go off, only the backlight (on again in deepStandbyEnded()). The page stays
     * until deepStandbyEnded() or the next start clears it.
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
        sicp.execute(() -> {
            if (set(page) != Reply.ACK || !scheduled(page))
            {
                Log.e(TAG, "deep standby: page " + SCHEDULE_PAGE + " not written as asked, only the backlight goes off");
                set(SICPDef.getSICPCommand(SICPCommand.SICP_COMMAND_SET_BACKLIGHT_DISABLE));
                return;
            }
            Log.i(TAG, String.format("deep standby until %tF %<tR", wake));
            set(off);
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
     * standby_mode deep: only the CMS switches the display, all scheduling pages 1…7 are disabled, also the ones set up
     * in the menu: the firmware switches exactly at the times of a page, an old page would switch the display off or on
     * beside the SMIL. From SICP 2.09 on with one command, Reset Scheduler (0x60 0x00), if afterwards every page reads
     * back disabled (0x5B; what "default values" are, the document does not say); otherwise, and before 2.09, every
     * page on its own (0x5A, page in the high nibble, 0 disabled, times 24:60, no source, no days).
     */
    @Override
    public void takeOverSchedule()
    {
        sicp.execute(() -> {
            if (sicp_version >= 209 && set(SICPDef.command((byte) 0x60, (byte) 0x00)) == Reply.ACK && allDisabled())
            {
                Log.i(TAG, "standby_mode deep: scheduling pages reset (0x60)");
                return;
            }
            boolean all = true;
            for (byte p = 1; p <= 7; ++p)
                all &= set(SICPDef.command((byte) 0x5A, (byte) (p << 4), NULL_HOUR, NULL_MINUTE, NULL_HOUR, NULL_MINUTE,
                                           (byte) 0x00, (byte) 0x00)) == Reply.ACK;
            Log.i(TAG, "standby_mode deep: scheduling pages 1-7 " + (all ? "disabled" : "not all acknowledged"));
        });
    }

    /// Every scheduling page reads back disabled (0x5B report: DATA[1] 0)
    private boolean allDisabled()
    {
        for (byte p = 1; p <= 7; ++p)
        {
            byte[] report = get(SICPDef.command((byte) 0x5B, p));
            if (report == null || report[3] != 0x5B || report.length < 6 || report[4] != 0x00)
            {
                Log.w(TAG, "after Reset Scheduler page " + p + " " + (report == null ? "no answer" : hex(report)));
                return false;
            }
        }
        return true;
    }

    /**
     * Reads page 7 back (0x5B) and compares it with the written command. The report: 0x5B, enabled, start hour and
     * minute, end hour and minute, source, working days (from SICP 2.05 on a tag after them), from the 4th byte on like
     * the command after its page byte.
     */
    private boolean scheduled(byte[] page)
    {
        byte[] report = get(SICPDef.command((byte) 0x5B, SCHEDULE_PAGE));
        if (report == null || report[3] != 0x5B)
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

    // ------------------------------------------------------------------ one command after the other

    /// A set command in the thread sicp: ACK, NACK or NAV (logged), NONE without an answer
    private Reply set(byte[] command)
    {
        byte[] answer = exchange(command, (byte) 0x00);
        Reply  reply  = answer == null || answer.length < 6 ? Reply.NONE
                        : answer[4] == 0x06 ? Reply.ACK
                        : answer[4] == 0x15 ? Reply.NACK
                        : answer[4] == 0x18 ? Reply.NAV : Reply.NONE;
        if (reply == Reply.NACK)
            Log.e(TAG, "SICP NACK (checksum, parameter or length wrong): " + hex(command));
        else if (reply == Reply.NAV)
            Log.w(TAG, "SICP NAV (this display does not have it): " + hex(command));
        else if (reply == Reply.NONE)
            Log.w(TAG, "SICP no answer: " + hex(command));
        return reply;
    }

    /// A get command in the thread sicp: its report, null for NACK, NAV or no answer
    private byte[] get(byte[] command)
    {
        byte[] answer = exchange(command, command[3]);
        if (answer != null && answer[3] == 0x00)
        {
            Log.w(TAG, "SICP " + (answer.length > 4 && answer[4] == 0x18 ? "NAV" : "NACK") + ": " + hex(command));
            return null;
        }
        return answer;
    }

    private byte[] get(SICPCommand command)
    {
        return get(SICPDef.getSICPCommand(command));
    }

    /**
     * Sends and waits ANSWER_MS for the answer with the code (DATA[0]) or for ACK/NACK/NAV (DATA[0] 0x00); answers
     * that came late for an earlier command are dropped, so is an ACK while a get command waits.
     */
    private byte[] exchange(byte[] command, byte code)
    {
        answers.clear();
        socket.sendSICPCommand(command);
        long deadline = SystemClock.uptimeMillis() + ANSWER_MS;
        try
        {
            for (long left = ANSWER_MS; left > 0; left = deadline - SystemClock.uptimeMillis())
            {
                byte[] answer = answers.poll(left, TimeUnit.MILLISECONDS);
                if (answer == null)
                    return null;
                boolean control = answer.length > 4 && answer[3] == 0x00;
                if (control && code != 0x00 && answer[4] == 0x06)
                    continue; // a late ACK of a set command: a get command is not acknowledged
                if (answer.length > 4 && (answer[3] == code || control))
                    return answer;
            }
        }
        catch (InterruptedException e)
        {
            Thread.currentThread().interrupt();
        }
        return null;
    }

    /// Runs in the thread sicp and waits for it (fetchDeviceInformation is called in a thread of its own)
    private void await(Runnable work)
    {
        try
        {
            sicp.submit(work).get();
        }
        catch (Exception e) // interrupted or the work failed: the information stays as it is
        {
            Log.w(TAG, "SICP: " + e);
        }
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
}
