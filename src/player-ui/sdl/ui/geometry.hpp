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
#ifndef PLAYER_UI_SDL_UI_GEOMETRY_HPP
#define PLAYER_UI_SDL_UI_GEOMETRY_HPP

#include <algorithm>

// Copied from the DJ workstation (src/player-ui/sdl), a shared library later.
namespace player::ui
{
    /// Position in window coordinates (points, not pixels)
    struct Point
    {
            float x = 0.0F;
            float y = 0.0F;
    };

    struct Rect
    {
            float x      = 0.0F;
            float y      = 0.0F;
            float width  = 0.0F;
            float height = 0.0F;

            [[nodiscard]] bool contains(Point point) const noexcept
            {
                return point.x >= x && point.x < x + width && point.y >= y && point.y < y + height;
            }

            /// The rectangle shrunk by margin on every side, never negative in size
            [[nodiscard]] Rect inset(float margin) const noexcept
            {
                return {x + margin, y + margin, std::max(0.0F, width - 2 * margin), std::max(0.0F, height - 2 * margin)};
            }
    };

    struct Color
    {
            unsigned char red   = 0;
            unsigned char green = 0;
            unsigned char blue  = 0;
            unsigned char alpha = 255;

            bool operator==(const Color &) const = default;
    };
}

#endif // PLAYER_UI_SDL_UI_GEOMETRY_HPP
