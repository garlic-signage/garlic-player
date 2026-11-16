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
#ifndef PLAYER_UI_SDL_UI_WIDGET_HPP
#define PLAYER_UI_SDL_UI_WIDGET_HPP

#include "canvas.hpp"
#include "geometry.hpp"

namespace player::ui
{
    /**
     * Base of the widgets (copied from the DJ workstation). A widget gets pointer events only from the pointer that
     * captured it on down (see PointerRouter), so it never has to sort out several fingers itself.
     */
    class Widget
    {
        public:
            Widget()                          = default;
            Widget(const Widget &)            = delete;
            Widget &operator=(const Widget &) = delete;
            virtual ~Widget()                 = default;

            [[nodiscard]] const Rect &bounds() const noexcept { return area; }
            void                      setBounds(const Rect &rect) noexcept { area = rect; }

            virtual void pointerDown(Point position) = 0;
            virtual void pointerMove(Point position) = 0;
            /// Also called when the pointer is cancelled
            virtual void pointerUp(Point position)  = 0;
            virtual void draw(Canvas &canvas) const = 0;

        private:
            Rect area;
    };
}

#endif // PLAYER_UI_SDL_UI_WIDGET_HPP
