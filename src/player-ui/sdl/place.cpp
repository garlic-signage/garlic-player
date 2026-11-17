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
#include "place.hpp"

#include <algorithm>

namespace player
{
    SDL_FRect place(const SDL_FRect &area, float width, float height, const std::string &fit,
                    const std::string &media_align)
    {
        float w = width;
        float h = height;
        if (width > 0 && height > 0)
        {
            float meet  = std::min(area.w / width, area.h / height);
            float cover = std::max(area.w / width, area.h / height);
            if (fit == "fill")
            {
                w = area.w;
                h = area.h;
            }
            else if (fit == "meet" || (fit == "meetbest" && (width > area.w || height > area.h)))
            {
                w = width * meet;
                h = height * meet;
            }
            else if (fit == "slice")
            {
                w = width * cover;
                h = height * cover;
            }
        }

        // mediaAlign: topLeft, topMid, topRight, midLeft, center, midRight, bottomLeft, bottomMid, bottomRight
        float fx = 0;
        float fy = 0;
        if (media_align == "center")
            fx = fy = 0.5F;
        else if (media_align == "topmid")
            fx = 0.5F;
        else if (media_align == "topright")
            fx = 1;
        else if (media_align == "midleft")
            fy = 0.5F;
        else if (media_align == "midright")
        {
            fx = 1;
            fy = 0.5F;
        }
        else if (media_align == "bottomleft")
            fy = 1;
        else if (media_align == "bottommid")
        {
            fx = 0.5F;
            fy = 1;
        }
        else if (media_align == "bottomright")
            fx = fy = 1;
        return {area.x + (area.w - w) * fx, area.y + (area.h - h) * fy, w, h};
    }
}
