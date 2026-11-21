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
 * dialogs ask for settings and information and wait for the answer. With a launcher the core is supervised: its
 * restarts and installations come to the Supervisor in the UI thread.
 */
public final class NativeCore
{
    static
    {
        System.loadLibrary("c++_shared");
        System.loadLibrary("core");
        System.loadLibrary("garlic-player");
    }

    /** Restarts and installations of the core (task reboot, firmwareUpdate), for the launcher */
    public interface Supervisor
    {
        void reboot(String task_id);
        void installSoftware(String path);
    }

    private final Handler    ui = new Handler(Looper.getMainLooper());
    private final Runnable   wake;
    private final Supervisor supervisor;
    private long             handle;

    /**
     * wake runs in the UI thread when the core has new events. index: empty for the one of the ini; uuid: of the
     * launcher, taken only if the ini has none; launcher: "philips-FB01.09" behind the version in the user agent, empty
     * without one; supervisor: null without launcher
     */
    public NativeCore(String ini, String cache, String logs, String index, String uuid, String launcher,
                      String ca_bundle, Runnable wake, Supervisor supervisor)
    {
        this.wake       = wake;
        this.supervisor = supervisor;
        handle          = create(ini, cache, logs, index, uuid, launcher, ca_bundle, (int) System.nanoTime(),
                                 supervisor != null);
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
    /** standby_mode of the configuration: partially, deep, no_standby */
    public String standbyMode() { return standbyMode(handle); }
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

    /** From the core thread (jni.cpp) */
    @SuppressWarnings("unused")
    private void onReboot(String task_id)
    {
        if (supervisor != null)
            ui.post(() -> supervisor.reboot(task_id));
    }

    /** From the core thread (jni.cpp) */
    @SuppressWarnings("unused")
    private void onInstall(String path)
    {
        if (supervisor != null)
            ui.post(() -> supervisor.installSoftware(path));
    }

    private native long create(String ini, String cache, String logs, String index, String uuid, String launcher,
                               String ca_bundle, int seed, boolean supervised);
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
    private static native String standbyMode(long handle);
    private static native void stopPlayback(long handle);
}
