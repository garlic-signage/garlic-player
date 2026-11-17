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
#ifndef PLAYER_UI_SDL_PLACE_HPP
#define PLAYER_UI_SDL_PLACE_HPP

#include <SDL3/SDL_rect.h>

#include <string>

namespace player
{
    /**
     * Where content of width × height goes in area for fit and mediaAlign, for images and videos alike (SMIL standard):
     * fill stretches, meet keeps the aspect inside, meetBest like meet but never larger than the original, slice
     * covers the area (cut by the clip), anything else keeps the original size. The result is placed by mediaAlign
     * (topLeft if unknown, garlic-lib); a larger result is cut on the sides the alignment points away from.
     */
    SDL_FRect place(const SDL_FRect &area, float width, float height, const std::string &fit,
                    const std::string &media_align);
}

#endif // PLAYER_UI_SDL_PLACE_HPP
