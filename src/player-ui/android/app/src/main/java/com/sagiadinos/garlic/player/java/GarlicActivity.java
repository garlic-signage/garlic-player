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
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.TextureView;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.webkit.CookieManager;
import android.webkit.WebStorage;
import android.webkit.WebView;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import com.sagiadinos.garlic.player.NativeCore;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.FileOutputStream;
import java.io.IOException;

/**
 * The Android player: Java on the core, like the sdl and the JS player. The core runs in its thread (NativeCore); its
 * events change the stage. Video and audio with Media3, web pages with the WebView of the system, all as views of one
 * hierarchy, so they stack by z-index (TextureView, not SurfaceView). Operated like every player: ten fast touches or
 * one held for 10 s open the menu, Ctrl+S, Ctrl+D, Ctrl+Q with a keyboard, other keys are access keys.
 *
 * Launcher 1.x or the Philips firmware like the qml player: uuid and content URL at the start, restarts and
 * installations of the core, the screen at adapi:blankScreen, the configuration and index the launcher sends, and
 * that the player ended on purpose.
 */
public class GarlicActivity extends Activity implements Media.Reports
{
    private static final String TAG = "garlic-player";
    private static final int    STORAGE_REQUEST = 1;

    private final Handler ui = new Handler(Looper.getMainLooper());
    private NativeCore    core;
    private Stage         stage;
    private LinearLayout  loading;
    private TextView      loading_text;
    private ProgressBar   loading_bar;
    private JSONObject    downloads    = new JSONObject();
    private boolean       media_shown;  // since the layout: "Loading" only before its first medium

    private LauncherInterface       launcher; // null without one
    private final BlankScreen       blank_screen    = new BlankScreen();
    private final BroadcastReceiver config_receiver = new ConfigReceiver(this::applyConfiguration);
    private final BroadcastReceiver index_receiver  = new SmilIndexReceiver(this::playIndex);

    // ten fast touches (< 500 ms apart) or one held for 10 s open the menu
    private long           released_at;
    private int            fast_touches;
    private final Runnable hold = this::openMenu;

    @Override
    protected void onCreate(Bundle state)
    {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON | WindowManager.LayoutParams.FLAG_FULLSCREEN);
        hideSystemBars();

        FrameLayout root = new FrameLayout(this);
        stage            = new Stage(this);
        root.addView(stage, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        root.addView(createLoading(), new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                                                                   ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER));
        setContentView(root);

        if (isPackageInstalled(GarlicLauncher.PACKAGE_NAME))
            launcher = new GarlicLauncher(getApplicationContext());
        else if (isPackageInstalled(PhilipsLauncher.PACKAGE_NAME))
            launcher = new PhilipsLauncher(getApplicationContext());
        blank_screen.setControl(launcher);
        register(config_receiver, ConfigReceiver.ACTION);
        register(index_receiver, SmilIndexReceiver.ACTION);

        if (Build.VERSION.SDK_INT >= 23 && checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED)
            requestPermissions(new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE}, STORAGE_REQUEST);
        else
            startCore(true);
    }

    @Override
    public void onRequestPermissionsResult(int request, String[] permissions, int[] results)
    {
        super.onRequestPermissionsResult(request, permissions, results);
        if (request == STORAGE_REQUEST)
            startCore(results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED);
    }

    @Override
    protected void onDestroy()
    {
        unregisterReceiver(config_receiver);
        unregisterReceiver(index_receiver);
        if (stage != null)
            stage.clearAll();
        if (core != null)
            core.destroy();
        core = null;
        super.onDestroy();
    }

    @Override
    public void onWindowFocusChanged(boolean focus)
    {
        super.onWindowFocusChanged(focus);
        if (focus)
            hideSystemBars();
    }

    private void hideSystemBars()
    {
        // the whole screen, also where navigation bar and display cutout are
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                                                         | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                                                         | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                                                         | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        if (Build.VERSION.SDK_INT >= 28)
            getWindow().getAttributes().layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
    }

    // ------------------------------------------------------------------ core

    /**
     * Without permission for the shared storage cache and logs stay inside the app (the launcher cannot update).
     * With a launcher its device information first, in a thread: Philips answers over the SICP socket, which may not be
     * connected yet; asked again for up to 5 s like the qml and the legacy player.
     */
    private void startCore(boolean storage)
    {
        if (launcher == null)
        {
            createCore(storage);
            return;
        }
        LauncherInterface asked = launcher;
        new Thread(() -> {
            long deadline = SystemClock.uptimeMillis() + 5000;
            do
            {
                asked.fetchDeviceInformation();
                if (!asked.getUUIDFromLauncher().isEmpty())
                    break;
                SystemClock.sleep(250);
            } while (SystemClock.uptimeMillis() < deadline);
            Log.i(TAG, "launcher: " + asked.getLauncherName() + " " + asked.getLauncherVersion() + " uuid: "
                       + asked.getUUIDFromLauncher());
            ui.post(() -> createCore(storage));
        }, "launcher").start();
    }

    /** The garlic-launcher gives name and content URL: the settings dialog shows them, but does not change them */
    private boolean identityLocked()
    {
        return launcher instanceof GarlicLauncher;
    }

    private void createCore(boolean storage)
    {
        if (core != null || isFinishing())
            return;
        if (!storage)
            Log.w(TAG, "no permission for the shared storage: cache and logs inside the app");
        Paths  paths = new Paths(this, storage);
        String index = "", uuid = "", fallback_index = "";
        NativeCore.Supervisor supervisor = null;
        if (launcher != null)
        {
            LauncherInterface l = launcher;
            uuid       = l.getUUIDFromLauncher(); // only for a new player: a stored uuid stays
            supervisor = new NativeCore.Supervisor()
            {
                @Override
                public void reboot(String task_id)
                {
                    Log.i(TAG, "launcher: reboot " + task_id);
                    l.rebootOS(task_id);
                }

                @Override
                public void installSoftware(String path)
                {
                    Log.i(TAG, "launcher: install " + path);
                    l.installSoftware(path);
                }
            };
            if (identityLocked())
                index = l.getContentUrlFromLauncher(); // the content URL of the garlic-launcher wins
            else
                fallback_index = l.getContentUrlFromLauncher(); // Philips: only a default
        }
        core = new NativeCore(paths.ini, paths.cache, paths.logs, index, uuid, Paths.caBundle(this), this::drain,
                              supervisor);
        String error = core.start();
        if (error != null)
        {
            Log.e(TAG, "garlic-player cannot start: " + error);
            showMessage("garlic-player cannot start: " + error);
            return;
        }
        drain();
        // without an index the settings dialog comes first, Cancel ends the player (garlic-lib); not when the
        // garlic-launcher gives the content URL: the player waits for its configuration
        boolean without_index = indexOf(core.settings()).isEmpty();
        if (without_index && !fallback_index.isEmpty())
        {
            try
            {
                JSONObject next = new JSONObject(core.settings());
                next.put("indexUri", fallback_index);
                without_index = core.changeSettings(next.toString()) != null;
            }
            catch (JSONException ignored)
            {
            }
        }
        if (without_index && !identityLocked())
            Dialogs.settings(this, core, true, false, this::finish);
    }

    private static String indexOf(String settings)
    {
        try
        {
            return new JSONObject(settings).optString("indexUri");
        }
        catch (JSONException error)
        {
            return "";
        }
    }

    /** The events of the core, in the UI thread after its wake */
    private void drain()
    {
        if (core == null)
            return;
        try
        {
            JSONArray events = new JSONArray(core.takeEvents());
            for (int i = 0; i < events.length(); ++i)
                handle(events.getJSONObject(i));
        }
        catch (JSONException error)
        {
            Log.e(TAG, "events of the core: " + error.getMessage());
        }
        updateLoading();
    }

    private void handle(JSONObject event) throws JSONException
    {
        long id = event.optLong("id");
        switch (event.getString("type"))
        {
            case "layout":
                stage.newLayout(event);
                media_shown = false; // a new playlist may wait for its files again
                break;
            case "background": stage.backgroundImage(event.optString("src"), event.optString("path")); break;
            case "start": start(event); break;
            case "stop":
                blank_screen.end(id);
                stage.stop(id);
                break;
            case "repeat": forEach(id, Media::replay); break;
            case "pause":
                for (Stage.Region region : stage.regions())
                {
                    Media medium = region.find(id);
                    if (medium != null)
                    {
                        medium.pause();
                        region.update();
                    }
                }
                break;
            case "resume":
                for (Stage.Region region : stage.regions())
                {
                    Media medium = region.find(id);
                    if (medium != null)
                    {
                        medium.resume();
                        region.raise(medium);
                    }
                }
                break;
            case "clearCaches": clearCaches(event.optBoolean("web")); break;
            case "screenshot": screenshot(event.optString("path")); break;
            case "downloads": downloads = event; break;
            case "deviceSettings": break; // master volume and brightness are for a launcher
            default: break;
        }
    }

    private interface Action { void run(Media medium); }

    private void forEach(long id, Action action)
    {
        for (Stage.Region region : stage.regions())
        {
            Media medium = region.find(id);
            if (medium != null)
                action.run(medium);
        }
    }

    /** One medium per region of the instance; only the first reports to the core */
    private void start(JSONObject event) throws JSONException
    {
        long    id      = event.getLong("id");
        String  kind    = event.getString("kind");
        String  source  = event.optString("source");
        String  label   = event.optString("label");
        if (kind.equals("command")) // garlic-lib only knew adapi:blankScreen, which needs the launcher
        {
            if (source.equalsIgnoreCase("adapi:blankScreen"))
                blank_screen.begin(id, core.standbyMode(), () -> core.remainingTime(id));
            else
            {
                Log.w(TAG, "unknown command " + source);
                mediaFailed(id);
            }
            return;
        }
        JSONArray regions = event.optJSONArray("regions");
        boolean   primary = true;
        for (int i = 0; regions != null && i < regions.length(); ++i)
        {
            JSONObject   definition = regions.getJSONObject(i);
            Stage.Region region     = stage.region(definition.getInt("index"));
            if (region == null)
                continue;
            if (!kind.equals("web") && !kind.equals("brush") && Media.missing(source))
            {
                if (primary)
                    mediaFailed(id); // one report per instance, also for a missing file
                primary = false;
                continue;
            }
            String fit   = definition.optString("fit");
            String align = definition.optString("align");
            Media  medium;
            switch (kind)
            {
                case "image": medium = new Media.Image(this, id, label, primary, this, source, fit, align); break;
                case "video":
                case "audio":
                    medium = new Media.AudioVideo(this, id, kind, label, primary, this, source, fit, align,
                                                  definition.optDouble("volume", 1));
                    break;
                case "web": medium = new Media.Web(this, id, label, primary, this, source); break;
                case "brush":
                    medium = new Media.Brush(this, id, label, primary, this,
                                             event.isNull("color") ? null : (int) event.optLong("color"));
                    break;
                default: continue;
            }
            primary = false;
            region.add(medium);
            medium.start();
        }
    }

    @Override
    public void finished(long id)
    {
        if (core != null)
            core.mediaFinished(id);
    }

    @Override
    public void failed(long id)
    {
        mediaFailed(id);
    }

    private void mediaFailed(long id)
    {
        if (core != null)
            core.mediaFailed(id);
    }

    // ------------------------------------------------------------------ launcher

    /** A configuration.xml the launcher sends, e.g. from a USB stick; also without a found launcher (legacy player) */
    private void applyConfiguration(String path)
    {
        if (core == null)
        {
            Log.w(TAG, "broadcast before the start, not taken: " + path);
            return;
        }
        if (!core.applyConfiguration(path))
            Log.w(TAG, "configuration not applied: " + path);
    }

    /** A SMIL index the launcher found, e.g. on a USB stick: played from now on */
    private void playIndex(String path)
    {
        if (core == null)
        {
            Log.w(TAG, "broadcast before the start, not taken: " + path);
            return;
        }
        core.playIndex(path);
    }

    /** Ends the player on purpose (menu, keys, debug dialog): the launcher shall not start it again */
    private void quit()
    {
        if (launcher != null)
            launcher.closedCorrectly();
        finishAndRemoveTask();
    }

    /** Android 13+ asks for the flag; the broadcasts of the launcher come from another app */
    private void register(BroadcastReceiver receiver, String action)
    {
        IntentFilter filter = new IntentFilter(action);
        if (Build.VERSION.SDK_INT >= 33)
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
        catch (PackageManager.NameNotFoundException error)
        {
            return false;
        }
    }

    // ------------------------------------------------------------------ tasks

    /** clear_webcache: the whole profile of the WebView (cache, cookies, local storage), then the core goes on */
    private void clearCaches(boolean web)
    {
        if (web)
        {
            WebStorage.getInstance().deleteAllData();
            CookieManager.getInstance().removeAllCookies(null);
            CookieManager.getInstance().flush();
            WebView temporary = new WebView(this);
            temporary.clearCache(true);
            temporary.destroy();
        }
        core.cachesCleared();
    }

    /** The screen as JPEG with quality 60 (garlic-lib); videos come from their TextureViews */
    private void screenshot(String path)
    {
        View   root   = stage.getRootView();
        Bitmap bitmap = Bitmap.createBitmap(Math.max(1, root.getWidth()), Math.max(1, root.getHeight()), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        root.draw(canvas);
        drawTextures(stage, canvas);
        try (FileOutputStream out = new FileOutputStream(path))
        {
            bitmap.compress(Bitmap.CompressFormat.JPEG, 60, out);
        }
        catch (IOException error)
        {
            Log.w(TAG, "screenshot: " + error.getMessage());
        }
        bitmap.recycle();
        core.screenshotTaken(path);
    }

    private void drawTextures(View view, Canvas canvas)
    {
        if (view instanceof TextureView && view.isShown())
        {
            Bitmap frame = ((TextureView) view).getBitmap();
            if (frame != null)
            {
                Rect place = new Rect();
                view.getGlobalVisibleRect(place);
                int[] at = new int[2];
                view.getLocationInWindow(at);
                canvas.drawBitmap(frame, null, new Rect(at[0], at[1], at[0] + view.getWidth(), at[1] + view.getHeight()), null);
                frame.recycle();
            }
        }
        else if (view instanceof ViewGroup)
            for (int i = 0; i < ((ViewGroup) view).getChildCount(); ++i)
                drawTextures(((ViewGroup) view).getChildAt(i), canvas);
    }

    // ------------------------------------------------------------------ loading

    private View createLoading()
    {
        loading = new LinearLayout(this);
        loading.setOrientation(LinearLayout.VERTICAL);
        loading.setGravity(Gravity.CENTER_HORIZONTAL);
        loading_text = new TextView(this);
        loading_text.setTextColor(Color.rgb(232, 232, 235));
        loading_text.setTextSize(20);
        loading.addView(loading_text);
        loading_bar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        loading_bar.setMax(1000);
        int width = Math.round(400 * getResources().getDisplayMetrics().density);
        loading.addView(loading_bar, new LinearLayout.LayoutParams(width, Math.round(12 * getResources().getDisplayMetrics().density)));
        loading.setVisibility(View.GONE);
        return loading;
    }

    /**
     * Like the sdl and the JS player: "Loading 1 / 7" while the media download and no medium was shown since the
     * layout; the bar follows the bytes of the first download, so it moves at once. Gone with the first medium.
     */
    private void updateLoading()
    {
        if (stage.showsMedia())
            media_shown = true;
        long total = downloads.optLong("total");
        if (total == 0 || media_shown)
        {
            loading.setVisibility(View.GONE);
            return;
        }
        long done        = downloads.optLong("done");
        long bytes_done  = downloads.optLong("bytesDone");
        long bytes_total = downloads.optLong("bytesTotal");
        loading_text.setText("Loading " + Math.min(done + 1, total) + " / " + total);
        double fraction = bytes_total > 0 ? (double) bytes_done / bytes_total : (double) done / total;
        loading_bar.setProgress((int) Math.round(1000 * Math.min(fraction, 1)));
        loading.setVisibility(View.VISIBLE);
    }

    private void showMessage(String text)
    {
        loading_text.setText(text);
        loading_bar.setVisibility(View.GONE);
        loading.setVisibility(View.VISIBLE);
    }

    // ------------------------------------------------------------------ input

    @Override
    public boolean dispatchTouchEvent(MotionEvent event)
    {
        switch (event.getActionMasked())
        {
            case MotionEvent.ACTION_DOWN:
                long now = SystemClock.uptimeMillis();
                fast_touches = now - released_at < 500 ? fast_touches + 1 : 1;
                ui.removeCallbacks(hold);
                ui.postDelayed(hold, 10000); // holding is meant for interactive content
                if (fast_touches >= 10)
                {
                    fast_touches = 0;
                    ui.removeCallbacks(hold);
                    openMenu();
                }
                break;
            case MotionEvent.ACTION_UP:
                released_at = SystemClock.uptimeMillis();
                ui.removeCallbacks(hold);
                activate(event.getX(), event.getY());
                break;
            case MotionEvent.ACTION_CANCEL:
                ui.removeCallbacks(hold);
                break;
            default: break;
        }
        return super.dispatchTouchEvent(event);
    }

    /** A touch on a region activates its top medium (activateEvent of the SMIL) */
    private void activate(float x, float y)
    {
        if (core == null)
            return;
        Stage.Region hit = null;
        for (int i = stage.getChildCount() - 1; i >= 0 && hit == null; --i)
        {
            View child = stage.getChildAt(i);
            if (child instanceof Stage.Region && ((Stage.Region) child).topMedium() != null)
            {
                Rect place = new Rect();
                child.getHitRect(place);
                if (place.contains((int) x, (int) y))
                    hit = (Stage.Region) child;
            }
        }
        if (hit != null)
            core.activated(hit.topMedium().id);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event)
    {
        if (event.getAction() != KeyEvent.ACTION_DOWN || core == null)
            return super.dispatchKeyEvent(event);
        if (event.isCtrlPressed())
        {
            switch (event.getKeyCode())
            {
                case KeyEvent.KEYCODE_S: openSettings(); return true;
                case KeyEvent.KEYCODE_D: openDebug(); return true;
                case KeyEvent.KEYCODE_Q: quit(); return true;
                default: return super.dispatchKeyEvent(event);
            }
        }
        int character = event.getUnicodeChar();
        if (character > 32 && character < 127 && !event.isAltPressed())
        {
            core.accessKey((char) character);
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    private void openMenu()
    {
        fast_touches = 0;
        Dialogs.menu(this, this::openSettings, this::openDebug, this::quit);
    }

    private void openSettings()
    {
        if (core != null)
            Dialogs.settings(this, core, false, identityLocked(), null);
    }

    private void openDebug()
    {
        if (core != null)
            Dialogs.debug(this, core, stage::playing);
    }
}
