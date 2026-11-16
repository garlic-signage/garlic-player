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
#include "paths.hpp"

#include <cstdlib>

namespace player
{
    namespace
    {
        constexpr const char *app = "garlic-player";

        std::string variable(const char *name)
        {
            const char *value = std::getenv(name);
            return value != nullptr ? value : "";
        }

        /// The variable, or home + fallback if it is not set
        std::string xdg(const char *name, const std::string &home, const char *fallback)
        {
            std::string value = variable(name);
            return value.empty() ? home + fallback : value;
        }
    }

    Paths standardPaths()
    {
        Paths paths;
#if defined(_WIN32)
        std::string roaming = variable("APPDATA");
        std::string local   = variable("LOCALAPPDATA");
        paths.ini_file      = roaming + "\\SmilControl\\garlic-player.ini";
        paths.cache_dir     = local + "\\" + app + "\\cache"; // the Qt players: not the program directory
        paths.data_dir      = local + "\\" + app;
        paths.log_dir       = paths.data_dir + "\\logs";
#elif defined(__APPLE__)
        std::string home = variable("HOME");
        paths.ini_file   = home + "/.config/SmilControl/garlic-player.ini";
        paths.cache_dir  = home + "/Library/Caches/" + app;
        paths.data_dir   = home + "/Library/Application Support/" + app;
        paths.log_dir    = paths.data_dir + "/logs";
#else
        std::string home = variable("HOME");
        paths.ini_file   = xdg("XDG_CONFIG_HOME", home, "/.config") + "/SmilControl/garlic-player.ini";
        paths.cache_dir  = xdg("XDG_CACHE_HOME", home, "/.cache") + "/" + app;
        paths.data_dir   = xdg("XDG_DATA_HOME", home, "/.local/share") + "/" + app;
        paths.log_dir    = paths.data_dir + "/logs";
#endif
        return paths;
    }
}
