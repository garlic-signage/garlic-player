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

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.media.MediaPlayer;
import android.net.Uri;
import android.util.DisplayMetrics;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.ImageView;

import androidx.annotation.NonNull;

import java.io.File;
import java.io.IOException;

/**
 * One medium in one region, like the Media of the sdl and the JS player. Only the first region of an instance reports
 * its end and errors to the core (primary). The view lies in the region; only the topmost medium with a picture is
 * visible (Region), audio covers nothing.
 */
abstract class Media
{
    interface Reports
    {
        void finished(long id);
        void failed(long id, int error, String reason);
    }

    /// Why a medium failed, smil::MediaError of the core: the events MEDIA_NOT_AVAILABLE, _OPEN_ERROR, _PLAYBACK_ERROR
    static final int NOT_AVAILABLE = 0;
    static final int OPEN          = 1;
    static final int PLAYBACK      = 2;

    final long    id;
    final String  kind;
    final String  label;
    final boolean primary;
    final Reports reports;
    boolean       paused;
    private boolean done;

    Media(long id, String kind, String label, boolean primary, Reports reports)
    {
        this.id      = id;
        this.kind    = kind;
        this.label   = label;
        this.primary = primary;
        this.reports = reports;
    }

    /** null for audio */
    abstract View view();
    abstract void start();
    abstract void stop();
    void pause() { paused = true; }
    void resume() { paused = false; }
    void replay() {}
    boolean hasPicture() { return view() != null; }

    void finished()
    {
        if (primary && !done)
        {
            done = true;
            reports.finished(id);
        }
    }

    void failed(int error, String why)
    {
        android.util.Log.w("garlic-player", "media failed: " + label + ": " + why);
        if (primary && !done)
        {
            done = true;
            reports.failed(id, error, why);
        }
    }

    /** A local path as file:// URL (query and fragment kept, like fileUrl of the sdl player), a URL as it is */
    static Uri uri(String source)
    {
        if (source.contains("://") || source.startsWith("data:"))
            return Uri.parse(source);
        return Uri.parse("file://" + Uri.encode(source, "/?#&=;,:+-._~%"));
    }

    // ------------------------------------------------------------------ image

    /** Decoded at most at the size of the screen: a 4K image on a device with 1 GB would take 32 MB per copy */
    static final class Image extends Media
    {
        private final FitFrame  frame;
        private final ImageView image;
        private final String    source;

        Image(Context context, long id, String label, boolean primary, Reports reports, String source, String fit,
              String align)
        {
            super(id, "image", label, primary, reports);
            this.source = source;
            frame       = new FitFrame(context, fit, align);
            image       = new ImageView(context);
            image.setScaleType(ImageView.ScaleType.FIT_XY);
            frame.addView(image);
        }

        @Override View view() { return frame; }

        @Override
        void start()
        {
            BitmapFactory.Options bounds = new BitmapFactory.Options();
            bounds.inJustDecodeBounds    = true;
            String path = source.startsWith("file://") ? Uri.parse(source).getPath() : source;
            BitmapFactory.decodeFile(path, bounds);
            if (bounds.outWidth <= 0 || bounds.outHeight <= 0)
            {
                failed(OPEN, "cannot load image");
                return;
            }
            DisplayMetrics screen = frame.getResources().getDisplayMetrics();
            int sample = 1;
            while (bounds.outWidth / (sample * 2) >= screen.widthPixels && bounds.outHeight / (sample * 2) >= screen.heightPixels)
                sample *= 2;
            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inSampleSize          = sample;
            Bitmap bitmap = BitmapFactory.decodeFile(path, options);
            if (bitmap == null)
            {
                failed(OPEN, "cannot decode image");
                return;
            }
            image.setImageBitmap(bitmap);
            frame.setContentSize(bounds.outWidth, bounds.outHeight); // fit by the original size
        }

        @Override
        void stop()
        {
            image.setImageDrawable(null);
        }
    }

    // ------------------------------------------------------------------ video and audio

    /**
     * android.media.MediaPlayer of the system (the decoders of the device), like the Qt player before. Video into a
     * SurfaceView: SurfaceFlinger shows each frame at its time (a TextureView takes it at the next draw of the app and
     * judders). The SurfaceView lies behind the window and punches a hole into it, so regions of a higher z-index lie
     * over the video, those of a lower one under it. The surface comes and goes with the view (SurfaceHolder).
     */
    static final class AudioVideo extends Media implements SurfaceHolder.Callback
    {
        private final Context     context;
        private final FitFrame    frame; // null for audio
        private final SurfaceView surface;
        private final String      source;
        private final float       volume;
        private MediaPlayer       player;
        private boolean           prepared;

        AudioVideo(Context context, long id, String kind, String label, boolean primary, Reports reports, String source,
                   String fit, String align, double volume)
        {
            super(id, kind, label, primary, reports);
            this.context = context;
            this.source  = source;
            this.volume  = (float) volume;
            if (kind.equals("video"))
            {
                frame   = new FitFrame(context, fit, align);
                surface = new SurfaceView(context);
                surface.getHolder().addCallback(this);
                frame.addView(surface);
            }
            else
            {
                frame   = null;
                surface = null;
            }
        }

        @Override View view() { return frame; }

        @Override
        void start()
        {
            player   = new MediaPlayer();
            prepared = false;
            player.setOnPreparedListener(mp -> {
                prepared = true;
                if (!paused)
                    mp.start();
            });
            player.setOnCompletionListener(mp -> finished());
            player.setOnErrorListener((mp, what, extra) -> {
                failed(prepared ? PLAYBACK : OPEN, "MediaPlayer error " + what + " " + extra);
                return true; // no completion afterwards
            });
            player.setOnVideoSizeChangedListener((mp, width, height) -> {
                if (frame != null && width > 0 && height > 0)
                    frame.setContentSize(width, height);
            });
            if (surface != null && surface.getHolder().getSurface().isValid())
                player.setDisplay(surface.getHolder());
            player.setVolume(volume, volume);
            try
            {
                player.setDataSource(context, uri(source));
            }
            catch (IOException | RuntimeException error)
            {
                failed(OPEN, "cannot open: " + error.getMessage());
                return;
            }
            player.prepareAsync();
        }

        @Override
        void stop()
        {
            if (player == null)
                return;
            player.release();
            player   = null;
            prepared = false;
        }

        @Override
        void pause()
        {
            super.pause();
            if (player != null && prepared && player.isPlaying())
                player.pause();
        }

        @Override
        void resume()
        {
            super.resume();
            if (player != null && prepared)
                player.start();
        }

        @Override
        void replay()
        {
            if (player == null || !prepared)
                return;
            player.seekTo(0);
            if (!paused)
                player.start();
        }

        @Override
        public void surfaceCreated(@NonNull SurfaceHolder holder)
        {
            if (player != null)
                player.setDisplay(holder);
        }

        @Override
        public void surfaceDestroyed(@NonNull SurfaceHolder holder)
        {
            if (player != null)
                player.setDisplay(null);
        }

        @Override public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {}
    }

    // ------------------------------------------------------------------ web page

    /**
     * The WebView of the system. Like the other players: local files may read remote data and other files, media play
     * without a gesture, no new windows, no scroll bars after loading. The duration comes from the core (dur).
     */
    static final class Web extends Media
    {
        private final WebView web;
        private final String  source;

        @SuppressLint("SetJavaScriptEnabled")
        Web(Context context, long id, String label, boolean primary, Reports reports, String source)
        {
            super(id, "web", label, primary, reports);
            this.source = source;
            web         = new WebView(context);
            web.setBackgroundColor(Color.WHITE); // opaque white like Chromium in a window (garlic-lib, Qt players)
            WebSettings settings = web.getSettings();
            settings.setJavaScriptEnabled(true);
            settings.setDomStorageEnabled(true);
            settings.setAllowFileAccess(true);
            settings.setAllowFileAccessFromFileURLs(true);
            settings.setAllowUniversalAccessFromFileURLs(true);
            settings.setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);
            settings.setMediaPlaybackRequiresUserGesture(false);
            settings.setSupportMultipleWindows(false);
            web.setVerticalScrollBarEnabled(false);
            web.setHorizontalScrollBarEnabled(false);
            web.setWebViewClient(new WebViewClient()
            {
                @Override
                public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request)
                {
                    return false; // links open in the region, never in another app
                }

                @Override
                public void onPageFinished(WebView view, String url)
                {
                    view.evaluateJavascript("document.body && (document.body.style.overflow = 'hidden');", null);
                }
            });
        }

        @Override View view() { return web; }

        @Override
        void start()
        {
            web.loadUrl(uri(source).toString());
        }

        @Override
        void stop()
        {
            web.stopLoading();
            web.loadUrl("about:blank");
            web.destroy();
        }

        @Override
        void pause()
        {
            super.pause();
            web.onPause();
        }

        @Override
        void resume()
        {
            super.resume();
            web.onResume();
        }

        @Override
        void replay()
        {
            web.reload();
        }
    }

    // ------------------------------------------------------------------ brush

    static final class Brush extends Media
    {
        private final View view;

        Brush(Context context, long id, String label, boolean primary, Reports reports, Integer color)
        {
            super(id, "brush", label, primary, reports);
            view = new View(context);
            view.setBackgroundColor(color != null ? color : Color.TRANSPARENT);
        }

        @Override View view() { return view; }
        @Override void start() {}
        @Override void stop() {}
    }

    /** A file that is no medium: MediaPlayer and BitmapFactory read local paths, so the cache file must be there */
    static boolean missing(String source)
    {
        return !source.contains("://") && !source.startsWith("data:") && !new File(source.split("[?#]")[0]).exists();
    }
}
