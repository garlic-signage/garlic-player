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
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.Shader;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

/**
 * What the player shows, like the stage of the JS player and the PlayerWindow of the sdl player: the regions of the
 * current layout by z-index (bottom first), placed relative to the screen, with background color and image; the media
 * of an instance in each of its regions, the newest on top. Only the topmost medium with a picture is visible.
 */
final class Stage extends FrameLayout
{
    /** One region: its media stacked, background color and image tiled like CSS repeat, repeatX, repeatY, noRepeat */
    static final class Region extends FrameLayout
    {
        final String      name;
        final double      left, top, width, height;
        final int         z;
        final Integer     color;
        final String      image_src;
        final String      repeat;
        final List<Media> media = new ArrayList<>();

        Region(Context context, JSONObject definition)
        {
            super(context);
            name      = definition.optString("name");
            left      = definition.optDouble("left", 0);
            top       = definition.optDouble("top", 0);
            width     = definition.optDouble("width", 1);
            height    = definition.optDouble("height", 1);
            z         = definition.optInt("z", 0);
            color     = definition.isNull("color") ? null : (int) definition.optLong("color");
            image_src = definition.optString("imageSrc");
            repeat    = definition.optString("repeat");
            setClipChildren(true);
            setBackground(background(color, definition.optString("image"), repeat));
        }

        void add(Media medium)
        {
            media.add(medium);
            View view = medium.view();
            if (view != null)
                addView(view, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
            update();
        }

        Media find(long id)
        {
            for (Media medium : media)
                if (medium.id == id)
                    return medium;
            return null;
        }

        void remove(long id)
        {
            Media medium = find(id);
            if (medium == null)
                return;
            medium.stop();
            media.remove(medium);
            if (medium.view() != null)
                removeView(medium.view());
            update();
        }

        void raise(Media medium)
        {
            media.remove(medium);
            media.add(medium);
            if (medium.view() != null)
                medium.view().bringToFront();
            update();
        }

        void clear()
        {
            for (Media medium : media)
                medium.stop();
            media.clear();
            removeAllViews();
        }

        /** The topmost medium with a picture is visible, a paused web page not (like the JS player) */
        void update()
        {
            Media top = null;
            for (Media medium : media)
                if (medium.hasPicture())
                    top = medium;
            for (Media medium : media)
                if (medium.view() != null)
                    medium.view().setVisibility(medium == top && !(medium.kind.equals("web") && medium.paused) ? VISIBLE : INVISIBLE);
        }

        Media topMedium()
        {
            return media.isEmpty() ? null : media.get(media.size() - 1);
        }
    }

    private final List<Region> regions = new ArrayList<>();
    private JSONObject         layout;

    Stage(Context context)
    {
        super(context);
        setBackgroundColor(Color.BLACK);
    }

    List<Region> regions() { return regions; }

    boolean showsMedia()
    {
        for (Region region : regions)
            if (!region.media.isEmpty())
                return true;
        return false;
    }

    /** "region: medium" for every medium shown, for the debug dialog */
    List<String> playing()
    {
        List<String> lines = new ArrayList<>();
        for (Region region : regions)
            for (Media medium : region.media)
                lines.add(region.name + ": " + medium.label);
        return lines;
    }

    /** A new index: the old media are stopped already (StopMedia come first), so only the regions go. */
    void newLayout(JSONObject next)
    {
        clearAll();
        layout = next;
        Integer root_color = next.isNull("rootColor") ? null : (int) next.optLong("rootColor");
        setBackground(background(root_color != null ? root_color : Color.BLACK, next.optString("rootImage"), next.optString("rootRepeat")));
        JSONArray definitions = next.optJSONArray("regions");
        List<Region> sorted = new ArrayList<>();
        for (int i = 0; definitions != null && i < definitions.length(); ++i)
            sorted.add(new Region(getContext(), definitions.optJSONObject(i)));
        regions.addAll(sorted); // indices of the core stay valid
        sorted.sort((a, b) -> Integer.compare(a.z, b.z)); // stable: document order within a z-index
        for (Region region : sorted)
            addView(region);
        requestLayout();
    }

    /** A background image that came after the layout */
    void backgroundImage(String src, String path)
    {
        if (layout == null)
            return;
        if (src.equals(layout.optString("rootImageSrc")))
        {
            Integer root_color = layout.isNull("rootColor") ? null : (int) layout.optLong("rootColor");
            setBackground(background(root_color != null ? root_color : Color.BLACK, path, layout.optString("rootRepeat")));
        }
        for (Region region : regions)
            if (src.equals(region.image_src))
                region.setBackground(background(region.color, path, region.repeat));
    }

    Region region(int index)
    {
        return index >= 0 && index < regions.size() ? regions.get(index) : null;
    }

    void stop(long id)
    {
        for (Region region : regions)
            region.remove(id);
    }

    void clearAll()
    {
        for (Region region : regions)
            region.clear();
        regions.clear();
        removeAllViews();
    }

    /** The regions by their part (0..1) of the screen: the layout is scaled to any screen like the other players */
    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b)
    {
        int w = r - l;
        int h = b - t;
        for (int i = 0; i < getChildCount(); ++i)
        {
            View child = getChildAt(i);
            if (child instanceof Region)
            {
                Region region = (Region) child;
                int x = (int) Math.round(region.left * w);
                int y = (int) Math.round(region.top * h);
                int rw = (int) Math.round(region.width * w);
                int rh = (int) Math.round(region.height * h);
                region.measure(MeasureSpec.makeMeasureSpec(rw, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(rh, MeasureSpec.EXACTLY));
                region.layout(x, y, x + rw, y + rh);
            }
            else
            {
                child.measure(MeasureSpec.makeMeasureSpec(w, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(h, MeasureSpec.EXACTLY));
                child.layout(0, 0, w, h);
            }
        }
    }

    /** Color, then the image at its own size from the top left corner, repeated like the CSS of the Qt players */
    static Drawable background(Integer color, String path, String repeat)
    {
        Drawable fill = new ColorDrawable(color != null ? color : Color.TRANSPARENT);
        if (path == null || path.isEmpty())
            return fill;
        Bitmap bitmap = BitmapFactory.decodeFile(path);
        if (bitmap == null)
            return fill;
        BitmapDrawable image = new BitmapDrawable(null, bitmap);
        image.setGravity(Gravity.TOP | Gravity.START);
        switch (repeat)
        {
            case "repeatx": image.setTileModeX(Shader.TileMode.REPEAT); break;
            case "repeaty": image.setTileModeY(Shader.TileMode.REPEAT); break;
            case "norepeat": break;
            default: image.setTileModeXY(Shader.TileMode.REPEAT, Shader.TileMode.REPEAT); break;
        }
        return new LayerDrawable(new Drawable[]{fill, image});
    }
}
