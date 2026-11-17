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
#ifndef PLAYER_UI_SDL_FILE_URL_HPP
#define PLAYER_UI_SDL_FILE_URL_HPP

#include <string>

namespace player
{
    /**
     * A local path as file:// URL for the WebView, percent-encoded like QUrl::fromLocalFile; an already encoded query or
     * fragment (e.g. "#toolbar=0" for the PDF viewer) stays. A URL with a scheme stays as it is.
     */
    std::string fileUrl(const std::string &source);
}

#endif // PLAYER_UI_SDL_FILE_URL_HPP
