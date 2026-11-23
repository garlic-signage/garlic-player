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

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Color;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.ForegroundColorSpan;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.ContextThemeWrapper;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import com.sagiadinos.garlic.player.NativeCore;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.function.Supplier;

/**
 * The menu and the two dialogs of every player (garlic-lib ConfigDialog and DebugInfos): reading, checking and storing
 * the settings is done by the core, the dialogs only show the fields.
 */
final class Dialogs
{
    private Dialogs() {}

    /** The backgrounds of the dialogs cover about 85 %, the playing content shines through slightly (like sdl) */
    private static final int PANEL_ALPHA = 217;

    /**
     * Dark Material dialogs everywhere: the theme of the activity leaves the dialog to the device, Philips makes it
     * light grey, and the information dialog writes in fixed light colors
     */
    private static final int DIALOG_THEME = android.R.style.Theme_Material_Dialog_Alert;

    /**
     * The activity with the density of the sdl player: max(density, screen height / 900), so menu and dialogs have the
     * same size at the same resolution on every platform, also on a 1080p screen that reports only 160 dpi.
     */
    static Context scaled(Activity activity)
    {
        DisplayMetrics screen = new DisplayMetrics();
        activity.getWindowManager().getDefaultDisplay().getRealMetrics(screen);
        float scale = Math.max(Math.max(1.0F, screen.density), screen.heightPixels / 900.0F);

        Configuration config = new Configuration();
        config.densityDpi    = Math.round(scale * DisplayMetrics.DENSITY_DEFAULT);
        ContextThemeWrapper context = new ContextThemeWrapper(activity, android.R.style.Theme_Black_NoTitleBar_Fullscreen);
        context.applyOverrideConfiguration(config);
        return context;
    }

    /**
     * After show(): the width of the sdl dialog in its points (at most the screen) and the content shining through
     * the dialog's backgrounds; buttons and fields stay opaque
     */
    private static void finish(AlertDialog dialog, Context ui, int points)
    {
        DisplayMetrics metrics = ui.getResources().getDisplayMetrics();
        int width = Math.min(Math.round(points * metrics.density), metrics.widthPixels);
        dialog.getWindow().setLayout(width, ViewGroup.LayoutParams.WRAP_CONTENT);
        translucent(dialog.getWindow().getDecorView());
    }

    private static void translucent(View view)
    {
        if (!(view instanceof Button) && !(view instanceof EditText) && view.getBackground() != null)
            view.getBackground().mutate().setAlpha(PANEL_ALPHA);
        if (view instanceof ViewGroup)
            for (int i = 0; i < ((ViewGroup) view).getChildCount(); ++i)
                translucent(((ViewGroup) view).getChildAt(i));
    }

    /** Settings, Information, Exit */
    static void menu(Activity activity, Runnable settings, Runnable debug, Runnable exit)
    {
        new AlertDialog.Builder(scaled(activity), DIALOG_THEME)
            .setItems(new CharSequence[]{"Settings", "Information", "Exit"}, (dialog, which) -> {
                if (which == 0)
                    settings.run();
                else if (which == 1)
                    debug.run();
                else
                    exit.run();
            })
            .show();
    }

    /**
     * Player name, content URL, HTTP server on/off and port; an error of the core keeps the dialog open. identity_locked:
     * name and content URL come from the garlic-launcher, they are only shown.
     */
    static void settings(Activity activity, NativeCore core, boolean required, boolean identity_locked, Runnable cancelled)
    {
        JSONObject current;
        try
        {
            current = new JSONObject(core.settings());
        }
        catch (JSONException error)
        {
            current = new JSONObject();
        }
        Context      ui     = scaled(activity);
        LinearLayout form   = column(ui);
        EditText     name   = field(ui, form, "Player name", current.optString("playerName"), false);
        EditText     index  = field(ui, form, "Content URL", current.optString("indexUri"), false);
        name.setEnabled(!identity_locked);
        index.setEnabled(!identity_locked);
        CheckBox     server = new CheckBox(ui);
        server.setText("HTTP server");
        server.setChecked(current.optBoolean("httpdEnabled", true));
        form.addView(server);
        EditText port  = field(ui, form, "Port", String.valueOf(current.optInt("httpdPort", 8080)), true);
        TextView error = new TextView(ui);
        error.setTextColor(Color.rgb(235, 90, 90));
        error.setVisibility(View.GONE);
        form.addView(error);

        AlertDialog dialog = new AlertDialog.Builder(ui, DIALOG_THEME)
            .setTitle("Settings")
            .setView(form)
            .setPositiveButton("OK", null)
            .setNegativeButton("Cancel", (d, w) -> {
                if (cancelled != null)
                    cancelled.run();
            })
            .setCancelable(!required)
            .create();
        dialog.setOnShowListener(d -> dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v -> {
            JSONObject next = new JSONObject();
            try
            {
                next.put("playerName", name.getText().toString().trim());
                next.put("indexUri", index.getText().toString().trim());
                next.put("httpdEnabled", server.isChecked());
                next.put("httpdPort", parsePort(port.getText().toString()));
            }
            catch (JSONException ignored)
            {
            }
            String failed = core.changeSettings(next.toString());
            if (failed == null)
            {
                dialog.dismiss();
                return;
            }
            error.setText(failed);
            error.setVisibility(View.VISIBLE);
        }));
        dialog.show();
        finish(dialog, ui, 640);
    }

    /**
     * Resources and identity, the running downloads and what plays; refreshed every second. One row of 30 points per
     * entry like the sdl dialog: the label left, the value cut with "…", the amount of a download right. CPU temperature
     * and throttling from the launcher (null: none) where it tells them, asked every 5 seconds like the core reads its
     * sensors; otherwise from the core.
     */
    static void debug(Activity activity, NativeCore core, Supplier<List<String>> playing, LauncherInterface launcher)
    {
        Context      ui      = scaled(activity);
        LinearLayout content = column(ui);
        ScrollView   scroll  = new ScrollView(ui);
        scroll.addView(content);

        Handler  timer   = new Handler(Looper.getMainLooper());
        Runnable[] tick  = new Runnable[1];
        int[]    seconds = {0};
        tick[0] = () -> {
            content.removeAllViews();
            if (launcher != null && seconds[0]++ % 5 == 0)
                launcher.fetchThermal(); // the answer comes for the next second
            Integer cpu        = launcher != null ? launcher.getTemperatureCpu() : null;
            Boolean throttling = launcher != null ? launcher.getThrottling() : null;
            for (String[] row : debugRows(core.debugInfo(), playing.get(), cpu, throttling))
                content.addView(debugRow(ui, row));
            timer.postDelayed(tick[0], 1000);
        };
        String version = "";
        try
        {
            version = new JSONObject(core.debugInfo()).optString("version");
        }
        catch (JSONException error)
        {
            // the title without the version
        }
        AlertDialog dialog = new AlertDialog.Builder(ui, DIALOG_THEME)
            .setTitle(version.isEmpty() ? "Information" : "Information · " + version)
            .setView(scroll)
            .setPositiveButton("Close", null) // the only button, centered; quitting is in the menu
            .create();
        dialog.setOnDismissListener(d -> timer.removeCallbacks(tick[0]));
        tick[0].run();
        dialog.show();
        finish(dialog, ui, 760);
    }

    /**
     * label (empty: continues the one above), value, amount (empty or "42 %  136.0 / 323.0 MiB"); launcher_cpu and
     * launcher_throttling replace the values of the core, null: the core's
     */
    static List<String[]> debugRows(String json, List<String> playing, Integer launcher_cpu, Boolean launcher_throttling)
    {
        List<String[]> rows = new ArrayList<>();
        try
        {
            JSONObject info = new JSONObject(json);
            rows.add(row("Playlist", info.optString("title").isEmpty() ? "-" : info.optString("title")));
            rows.add(row("User agent", info.optString("userAgent")));
            rows.add(row("Disk (cache)", freeOf(info, "diskFree", "diskTotal")));
            rows.add(row("System memory", freeOf(info, "memoryFree", "memoryTotal")));
            long used = info.optLong("memoryUsed");
            rows.add(row("Player memory", used == 0 ? "-"
                : bytes(used) + ", max " + bytes(info.optLong("memoryMax")) + " at " + info.optString("memoryMaxTime")));
            int threads = info.optInt("threads");
            rows.add(row("Threads", threads == 0 ? "-" : threads + ", max " + info.optInt("threadsMax")));
            // Android: the player process only, MediaCodec and the WebView renderer run in processes of the system;
            // the system load stays "-", Android 8+ does not let an app read /proc/stat
            rows.add(row("CPU", "Player " + (info.isNull("cpu") ? "-"
                : info.optInt("cpu") + " %" + (info.isNull("cpuMax") ? ""
                    : ", max " + info.optInt("cpuMax") + " % at " + info.optString("cpuMaxTime") + " (5 min)"))
                + " · System " + (info.isNull("cpuSystem") ? "-" : info.optInt("cpuSystem") + " %")));
            // a red or green circle after the text, "-" where neither launcher nor Android tell it (core: before 11)
            String cpu = launcher_cpu != null ? launcher_cpu + " °C" : celsius(info, "temperatureCpu");
            Boolean throttling = launcher_throttling != null ? launcher_throttling
                : info.isNull("throttling") ? null : info.optBoolean("throttling");
            String temperatures = "CPU " + cpu + " · RAM " + celsius(info, "temperatureRam")
                + " · SSD " + celsius(info, "temperatureSsd") + " · Throttling ";
            rows.add(throttling == null ? row("Temperature", temperatures + "-")
                : new String[]{"Temperature", temperatures + "●", "", throttling ? "red" : "green"});
            rows.add(row("Uptime", "Player " + duration(info, "uptimePlayer")
                + " · System " + duration(info, "uptimeSystem")));
            JSONArray downloads = info.optJSONArray("downloads");
            boolean   first     = true;
            for (int i = 0; downloads != null && i < downloads.length(); ++i)
            {
                JSONObject d        = downloads.getJSONObject(i);
                long       received = d.optLong("received");
                long       total    = d.optLong("total");
                String amount = total == 0 ? mib(received) + " MiB"
                    : (received * 100 / total) + " %  " + mib(received) + " / " + mib(total) + " MiB";
                rows.add(new String[]{first ? "Downloading" : "", d.optString("name"), amount});
                first = false;
            }
            long waiting = info.optLong("downloadsWaiting");
            if (waiting > 0)
                rows.add(row(first ? "Downloading" : "", "+" + waiting + " waiting"));
            else if (first)
                rows.add(row("Downloading", "-"));
        }
        catch (JSONException error)
        {
            rows.add(row("Error", error.getMessage()));
        }
        if (playing.isEmpty())
            rows.add(row("Playing", "-"));
        for (int i = 0; i < playing.size(); ++i)
            rows.add(row(i == 0 ? "Playing" : "", playing.get(i)));
        return rows;
    }

    /// "2d 3h 15m", "3h 15m", "15m"; "-" if the core does not know it
    private static String duration(JSONObject info, String key)
    {
        if (info.isNull(key) || !info.has(key))
            return "-";
        long   minutes = info.optLong(key) / 60;
        long   days    = minutes / (24 * 60);
        long   hours   = minutes / 60 % 24;
        String text    = minutes % 60 + "m";
        if (days > 0 || hours > 0)
            text = hours + "h " + text;
        if (days > 0)
            text = days + "d " + text;
        return text;
    }

    private static String celsius(JSONObject info, String key)
    {
        return info.isNull(key) ? "-" : info.optInt(key) + " °C";
    }

    private static String[] row(String label, String value)
    {
        return new String[]{label, value, ""};
    }

    private static View debugRow(Context ui, String[] row)
    {
        float        density = ui.getResources().getDisplayMetrics().density;
        LinearLayout line    = new LinearLayout(ui);
        line.setOrientation(LinearLayout.HORIZONTAL);
        line.setGravity(Gravity.CENTER_VERTICAL);
        line.setMinimumHeight(Math.round(30 * density));

        TextView label = debugText(ui, row[0], Color.rgb(150, 152, 158));
        line.addView(label, new LinearLayout.LayoutParams(Math.round(150 * density), ViewGroup.LayoutParams.WRAP_CONTENT));
        TextView value = debugText(ui, row[1], Color.rgb(232, 232, 235));
        if (row.length > 3) // the last character in a color: the circle of the throttling
        {
            SpannableString colored = new SpannableString(row[1]);
            int             color   = row[3].equals("red") ? Color.rgb(235, 90, 90) : Color.rgb(0, 190, 90);
            colored.setSpan(new ForegroundColorSpan(color), row[1].length() - 1, row[1].length(),
                            Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            value.setText(colored);
        }
        value.setEllipsize(TextUtils.TruncateAt.END);
        line.addView(value, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        if (!row[2].isEmpty())
        {
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                                                                             ViewGroup.LayoutParams.WRAP_CONTENT);
            params.leftMargin = Math.round(16 * density);
            line.addView(debugText(ui, row[2], Color.rgb(232, 232, 235)), params);
        }
        return line;
    }

    private static TextView debugText(Context ui, String text, int color)
    {
        TextView view = new TextView(ui);
        view.setText(text);
        view.setTextColor(color);
        view.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        view.setSingleLine(true);
        return view;
    }

    private static String freeOf(JSONObject info, String free, String total)
    {
        if (info.isNull(free) || info.isNull(total))
            return "-";
        return bytes(info.optLong(free)) + " free of " + bytes(info.optLong(total));
    }

    /** Like QLocale::formattedDataSize: "1.50 GiB" */
    static String bytes(long value)
    {
        String[] units = {"bytes", "KiB", "MiB", "GiB", "TiB"};
        double   size  = value;
        int      unit  = 0;
        while (size >= 1024 && unit < 4)
        {
            size /= 1024;
            ++unit;
        }
        return unit == 0 ? value + " bytes" : String.format(Locale.ROOT, "%.2f %s", size, units[unit]);
    }

    private static String mib(long value)
    {
        return String.format(Locale.ROOT, "%.1f", value / (1024.0 * 1024.0));
    }

    private static int parsePort(String text)
    {
        try
        {
            return Integer.parseInt(text.trim());
        }
        catch (NumberFormatException error)
        {
            return 0; // the core rejects it with its message
        }
    }

    private static LinearLayout column(Context context)
    {
        LinearLayout layout = new LinearLayout(context);
        layout.setOrientation(LinearLayout.VERTICAL);
        int padding = Math.round(20 * context.getResources().getDisplayMetrics().density);
        layout.setPadding(padding, padding / 2, padding, 0);
        return layout;
    }

    private static EditText field(Context context, ViewGroup form, String label, String value, boolean number)
    {
        TextView caption = new TextView(context);
        caption.setText(label);
        form.addView(caption);
        EditText edit = new EditText(context);
        edit.setText(value);
        edit.setSingleLine(true);
        edit.setGravity(Gravity.START);
        if (number)
            edit.setInputType(InputType.TYPE_CLASS_NUMBER);
        form.addView(edit);
        return edit;
    }
}
