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
import android.view.View;
import android.widget.FrameLayout;

/**
 * Places its one child by fit and mediaAlign of the SMIL like place() of the sdl player, for images and videos alike
 * (SMIL standard): fill stretches, meet keeps the aspect inside, meetBest like meet but never larger, slice covers
 * (cut by the region), without fit the original size; then placed by mediaAlign. The region cuts what lies outside.
 */
final class FitFrame extends FrameLayout
{
    private final String fit;
    private final String align;
    private int          content_width;
    private int          content_height;

    FitFrame(Context context, String fit, String align)
    {
        super(context);
        this.fit   = fit;
        this.align = align;
        setClipChildren(true);
    }

    /** The size of the content in pixels (image, video), 0 while unknown */
    void setContentSize(int width, int height)
    {
        content_width  = width;
        content_height = height;
        requestLayout();
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom)
    {
        if (getChildCount() == 0)
            return;
        View  child  = getChildAt(0);
        float area_w = right - left;
        float area_h = bottom - top;
        float w      = content_width;
        float h      = content_height;
        if (w <= 0 || h <= 0)
        {
            w = area_w;
            h = area_h;
        }
        else
        {
            float meet  = Math.min(area_w / w, area_h / h);
            float cover = Math.max(area_w / w, area_h / h);
            if (fit.equals("fill"))
            {
                w = area_w;
                h = area_h;
            }
            else if (fit.equals("meet") || (fit.equals("meetbest") && (w > area_w || h > area_h)))
            {
                w *= meet;
                h *= meet;
            }
            else if (fit.equals("slice"))
            {
                w *= cover;
                h *= cover;
            }
        }

        // mediaAlign: topLeft, topMid, topRight, midLeft, center, midRight, bottomLeft, bottomMid, bottomRight
        float fx = 0;
        float fy = 0;
        switch (align)
        {
            case "center": fx = 0.5f; fy = 0.5f; break;
            case "topmid": fx = 0.5f; break;
            case "topright": fx = 1; break;
            case "midleft": fy = 0.5f; break;
            case "midright": fx = 1; fy = 0.5f; break;
            case "bottomleft": fy = 1; break;
            case "bottommid": fx = 0.5f; fy = 1; break;
            case "bottomright": fx = 1; fy = 1; break;
            default: break;
        }
        int x = Math.round((area_w - w) * fx);
        int y = Math.round((area_h - h) * fy);
        int cw = Math.round(w);
        int ch = Math.round(h);
        child.measure(MeasureSpec.makeMeasureSpec(cw, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(ch, MeasureSpec.EXACTLY));
        child.layout(x, y, x + cw, y + ch);
    }
}
