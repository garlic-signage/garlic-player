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
#include "pointer_router.hpp"

#include <algorithm>
#include <ranges>

namespace player::ui
{
    void PointerRouter::handle(const PointerEvent &event)
    {
        if (event.phase == PointerPhase::Down)
        {
            Widget *widget = widgetAt(event.position);
            if (widget != nullptr && !isCaptured(*widget))
            {
                captures[event.pointer] = widget;
                widget->pointerDown(event.position);
            }
            return;
        }
        auto capture = captures.find(event.pointer);
        if (capture == captures.end())
            return;
        if (event.phase == PointerPhase::Move)
        {
            capture->second->pointerMove(event.position);
            return;
        }
        Widget *widget = capture->second;
        captures.erase(capture);
        widget->pointerUp(event.position);
    }

    bool PointerRouter::isCaptured(const Widget &widget) const noexcept
    {
        return std::ranges::any_of(captures, [&widget](const auto &capture) { return capture.second == &widget; });
    }

    Widget *PointerRouter::widgetAt(Point position) const noexcept
    {
        for (Widget *widget : std::views::reverse(widgets))
            if (widget->bounds().contains(position))
                return widget;
        return nullptr;
    }
}
