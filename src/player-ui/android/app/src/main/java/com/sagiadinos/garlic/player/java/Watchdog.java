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

import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;

/**
 * Notices a hanging UI thread. A thread of its own posts a beat to the UI thread every 5 s; when none has run for
 * 30 s, it calls hung once, in its own thread.
 *
 * Seen on a Philips BDL4550D: MediaPlayer.release() waited for the media server of the firmware, which hung after a
 * decoder did not start. The screen stayed black, no dialog opened, Android noticed it only at the next key (ANR).
 * A new player hangs again in the same media server, only a restart of the device helps.
 *
 * Not a Service: its callbacks would run in the hanging UI thread. Uptime clock: deep sleep is no hang.
 */
final class Watchdog
{
    static final long BEAT_MS    = 5000;
    static final long TIMEOUT_MS = 30000;

    private final Handler  ui = new Handler(Looper.getMainLooper());
    private final Runnable hung;
    private volatile long  last_beat;
    private Thread         thread;

    Watchdog(Runnable hung)
    {
        this.hung = hung;
    }

    /// In the UI thread
    void start()
    {
        if (thread != null)
            return;
        last_beat = SystemClock.uptimeMillis();
        thread    = new Thread(this::watch, "watchdog");
        thread.setDaemon(true);
        thread.start();
    }

    /// In the UI thread
    void stop()
    {
        if (thread == null)
            return;
        thread.interrupt();
        thread = null;
    }

    private void watch()
    {
        while (!Thread.currentThread().isInterrupted())
        {
            ui.post(() -> last_beat = SystemClock.uptimeMillis());
            try
            {
                Thread.sleep(BEAT_MS);
            }
            catch (InterruptedException stopped)
            {
                return;
            }
            if (SystemClock.uptimeMillis() - last_beat > TIMEOUT_MS)
            {
                hung.run();
                return;
            }
        }
    }

    /// Where the UI thread stands, for the log: the topmost frames
    static String uiStack()
    {
        StackTraceElement[] frames = Looper.getMainLooper().getThread().getStackTrace();
        StringBuilder       text   = new StringBuilder();
        for (int i = 0; i < Math.min(frames.length, 8); ++i)
            text.append(i == 0 ? "" : " < ").append(frames[i].getClassName()).append('.')
                .append(frames[i].getMethodName()).append(':').append(frames[i].getLineNumber());
        return text.toString();
    }
}
