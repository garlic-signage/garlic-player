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
#ifndef PLAYER_UI_SDL_UI_CANVAS_HPP
#define PLAYER_UI_SDL_UI_CANVAS_HPP

#include "geometry.hpp"

#include <optional>
#include <string_view>

namespace player::ui
{
    /// What widgets draw on. Keeps the widgets free of SDL. Copied from the DJ workstation, without pictures and the
    /// large text of its time display.
    class Canvas
    {
        public:
            Canvas()                          = default;
            Canvas(const Canvas &)            = delete;
            Canvas &operator=(const Canvas &) = delete;
            virtual ~Canvas()                 = default;

            virtual void fillRect(const Rect &rect, Color color) = 0;

            /// A rectangle with rounded corners, filled with a vertical gradient from top to bottom (the same color
            /// twice is flat)
            virtual void fillRoundedRect(const Rect &rect, float radius, Color top, Color bottom) = 0;

            /// One line of UTF-8 text, position is its top left corner; the line is textHeight() high
            virtual void text(Point position, std::string_view text, Color color) = 0;

            /// Like text(), but cut with "…" if it is wider than max_width
            virtual void textElided(Point position, std::string_view text, Color color, float max_width) = 0;

            [[nodiscard]] virtual float textHeight() const noexcept                     = 0;
            [[nodiscard]] virtual float textWidth(std::string_view text) const noexcept = 0;

            /// Drawing outside clip is cut off until the next setClip(); std::nullopt draws everywhere again
            virtual void setClip(std::optional<Rect> clip) = 0;
    };
}

#endif // PLAYER_UI_SDL_UI_CANVAS_HPP
