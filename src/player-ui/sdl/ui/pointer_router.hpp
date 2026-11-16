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
#ifndef PLAYER_UI_SDL_UI_POINTER_ROUTER_HPP
#define PLAYER_UI_SDL_UI_POINTER_ROUTER_HPP

#include "widget.hpp"

#include <compare>
#include <cstdint>
#include <map>
#include <vector>

namespace player::ui
{
    /// One finger on one touch device, or the mouse. Stays the same from down to up.
    struct PointerId
    {
            std::uint64_t device = 0;
            std::uint64_t finger = 0;

            auto operator<=>(const PointerId &) const = default;
    };

    enum class PointerPhase : std::uint8_t
    {
        Down,
        Move,
        Up
    };

    /// Mouse and touch input in one form, so the widgets do not care where it came from
    struct PointerEvent
    {
            PointerId    pointer;
            PointerPhase phase = PointerPhase::Down;
            Point        position;
    };

    /**
     * Every pointer captures the widget under it on down and keeps it until up (copied from the DJ workstation).
     * Moves and the up of a pointer go to its widget even outside the widget's bounds.
     */
    class PointerRouter
    {
        public:
            /// Widgets added later lie on top. The widgets must outlive the router.
            void add(Widget &widget) { widgets.push_back(&widget); }
            void handle(const PointerEvent &event);

        private:
            [[nodiscard]] Widget *widgetAt(Point position) const noexcept;
            [[nodiscard]] bool    isCaptured(const Widget &widget) const noexcept;

            std::vector<Widget *>         widgets;
            std::map<PointerId, Widget *> captures;
    };
}

#endif // PLAYER_UI_SDL_UI_POINTER_ROUTER_HPP
