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
#ifndef PLAYER_UI_SDL_PATHS_HPP
#define PLAYER_UI_SDL_PATHS_HPP

#include <string>

namespace player
{
    /**
     * The places the Qt players use (QSettings IniFormat UserScope "SmilControl/garlic-player", QStandardPaths
     * CacheLocation and AppLocalDataLocation of "garlic-player"), so the SDL player shares settings, cache and logs
     * with them and with garlic-lib.
     *
     *   Linux    ~/.config/SmilControl/garlic-player.ini, ~/.cache/garlic-player, ~/.local/share/garlic-player/logs
     *            (the XDG variables if set)
     *   macOS    ~/.config/SmilControl/garlic-player.ini, ~/Library/Caches/garlic-player,
     *            ~/Library/Application Support/garlic-player/logs
     *   Windows  %APPDATA%\SmilControl\garlic-player.ini, %LOCALAPPDATA%\garlic-player\cache,
     *            %LOCALAPPDATA%\garlic-player\logs
     */
    struct Paths
    {
        std::string ini_file;
        std::string cache_dir;
        std::string log_dir;
    };

    Paths standardPaths();
}

#endif // PLAYER_UI_SDL_PATHS_HPP
