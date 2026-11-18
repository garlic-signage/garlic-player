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
package com.sagiadinos.garlic.player;

import android.os.Handler;
import android.os.Looper;

/**
 * The core (libgarlic-player.so, native/jni.cpp) in a thread of its own. Its events come as a JSON array from
 * takeEvents(), after the wake runnable ran in the UI thread. Calls into the core are posted to its thread; the
 * dialogs ask for settings and debug infos and wait for the answer.
 */
public final class NativeCore
{
    static
    {
        System.loadLibrary("c++_shared");
        System.loadLibrary("core");
        System.loadLibrary("garlic-player");
    }

    private final Handler  ui = new Handler(Looper.getMainLooper());
    private final Runnable wake;
    private long           handle;

    /** wake runs in the UI thread when the core has new events */
    public NativeCore(String ini, String cache, String logs, String index, String ca_bundle, Runnable wake)
    {
        this.wake = wake;
        handle    = create(ini, cache, logs, index, ca_bundle, (int) System.nanoTime());
    }

    /** null, or the error text of the start */
    public String start()
    {
        String error = start(handle);
        return error.isEmpty() ? null : error;
    }

    public String takeEvents() { return takeEvents(handle); }

    public void mediaFinished(long id) { mediaFinished(handle, id); }
    public void mediaFailed(long id) { mediaFailed(handle, id); }
    public void activated(long id) { activated(handle, id); }
    public void accessKey(char key) { accessKey(handle, key); }
    public void screenshotTaken(String path) { screenshotTaken(handle, path); }
    public void cachesCleared() { cachesCleared(handle); }
    public void playIndex(String uri) { playIndex(handle, uri); }
    public boolean applyConfiguration(String path) { return applyConfiguration(handle, path); }
    public String settings() { return settings(handle); }
    /** null, or the error text of the core */
    public String changeSettings(String json)
    {
        String error = changeSettings(handle, json);
        return error.isEmpty() ? null : error;
    }
    public String debugInfo() { return debugInfo(handle); }
    public long remainingTime(long id) { return remainingTime(handle, id); }
    public void stopPlayback() { stopPlayback(handle); }

    /** Ends the core thread; the object is unusable afterwards */
    public void destroy()
    {
        if (handle == 0)
            return;
        destroy(handle);
        handle = 0;
    }

    /** From the core thread (jni.cpp) */
    @SuppressWarnings("unused")
    private void onWake()
    {
        ui.post(wake);
    }

    private native long create(String ini, String cache, String logs, String index, String ca_bundle, int seed);
    private static native String start(long handle);
    private static native String takeEvents(long handle);
    private static native void destroy(long handle);
    private static native void mediaFinished(long handle, long id);
    private static native void mediaFailed(long handle, long id);
    private static native void activated(long handle, long id);
    private static native void accessKey(long handle, char key);
    private static native void screenshotTaken(long handle, String path);
    private static native void cachesCleared(long handle);
    private static native void playIndex(long handle, String uri);
    private static native boolean applyConfiguration(long handle, String path);
    private static native String settings(long handle);
    private static native String changeSettings(long handle, String json);
    private static native String debugInfo(long handle);
    private static native long remainingTime(long handle, long id);
    private static native void stopPlayback(long handle);
}
