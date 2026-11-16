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
#ifndef PLAYER_UI_SDL_COLOR_HPP
#define PLAYER_UI_SDL_COLOR_HPP

#include "ui/geometry.hpp"

#include <optional>
#include <string_view>

namespace player
{
    /**
     * A color of the SMIL (backgroundColor, brush color) as the Qt players took it through their style sheets:
     * "transparent", "#rgb", "#rrggbb", "#aarrggbb" (alpha first, like QColor), "rgb(r, g, b)", "rgba(r, g, b, a)"
     * with a from 0 to 1 or 0 to 255, and the CSS color names, case ignored. std::nullopt for anything else: Qt then
     * drew nothing, so the caller draws nothing too.
     */
    std::optional<ui::Color> parseColor(std::string_view text);
}

#endif // PLAYER_UI_SDL_COLOR_HPP
