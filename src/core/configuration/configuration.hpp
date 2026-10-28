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
#ifndef GARLIC_CORE_CONFIGURATION_CONFIGURATION_HPP
#define GARLIC_CORE_CONFIGURATION_CONFIGURATION_HPP

#include "common/clock.hpp"
#include "common/file_system.hpp"
#include "configuration/ini_file.hpp"
#include "httpd/httpd_settings.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace garlic::core::configuration
{
    inline constexpr std::string_view standby_none      = "no_standby";
    inline constexpr std::string_view standby_partially = "partially";
    inline constexpr std::string_view standby_deep      = "deep";

    /**
     * Configuration of the player, stored in garlic-player.ini (compatible with the QSettings files of garlic-lib).
     *
     * The frontend passes the path of the ini file and the platform paths (cache, logs). Every change is written to
     * the file immediately; errors are kept in lastError() and never stop the player.
     */
    class Configuration
    {
        public:
            Configuration(common::FileSystem &fs, const common::Clock &clock, std::string ini_path);

            /// Reads the ini file. A missing file is no error. Creates uuid and player name if they are missing.
            std::expected<void, std::string> load();

            // --- any key, e.g. "audio/volume", "time/server"
            std::string value(std::string_view key) const;
            void        setValue(std::string key, std::string value);

            // --- identity
            std::string uuid() const { return value("uuid"); }
            std::string playerName() const { return value("player_name"); }
            void        setPlayerName(std::string name) { setValue("player_name", std::move(name)); }

            // --- SMIL index
            std::string indexUri() const { return value("index_uri"); }
            void        setIndexUri(std::string uri) { setValue("index_uri", std::move(uri)); }

            /// URI from the command line wins over the stored one and is stored.
            void determineIndexUri(std::string_view command_line_uri);

            /// Directory of the index: "https://domain.tld/path/" or "/path/to/".
            std::string indexPath() const;

            /// Checks an index URI as given by the user. Returns the URI to use (file:// is removed) or the error text.
            static std::expected<std::string, std::string> validateIndexUri(std::string_view uri);

            std::string lastPlayedIndexPath() const { return value("last_played_index_path"); }
            void setLastPlayedIndexPath(std::string path) { setValue("last_played_index_path", std::move(path)); }

            // --- operation
            std::string standbyMode() const { return value("standby_mode"); }
            void        setStandbyMode(std::string mode) { setValue("standby_mode", std::move(mode)); }
            std::string rebootDays() const { return value("schedule/reboot_days"); }
            void        setRebootDays(std::string days) { setValue("schedule/reboot_days", std::move(days)); }
            std::string rebootTime() const { return value("schedule/reboot_time"); }
            void        setRebootTime(std::string time) { setValue("schedule/reboot_time", std::move(time)); }

            /// Token for the REST API. Valid for one day, a new one is created after it expired.
            std::string determineApiAccessToken(std::string_view username, std::string_view password);
            std::string apiAccessToken() const { return value("api_access_token"); }

            // --- settings of the modules
            /// HTTP server from the group [httpd]; defaults for missing keys, upload_dir is the cache directory.
            httpd::HttpdSettings httpd() const;

            // --- given by the frontend, not stored
            void               setVersion(std::string v) { version = std::move(v); }
            void               setCacheDir(std::string dir) { cache_dir = std::move(dir); }
            void               setLogDir(std::string dir) { log_dir = std::move(dir); }
            const std::string &cacheDir() const noexcept { return cache_dir; }
            const std::string &logDir() const noexcept { return log_dir; }

            /// "linux", "windows", "macOS", "android", "iOS", "wasm", "esp32", …
            static std::string_view os();

            /// A new random UUID (version 4), e.g. for unique upload names of the reports.
            static std::string createUuid();

            /// "GAPI/1.0 (UUID:…; NAME:…) garlic-linux/1.2.3 (MODEL:Garlic)"
            std::string userAgent() const;

            const std::string &lastError() const noexcept { return last_error; }

        private:
            void save();

            common::FileSystem  &fs;
            const common::Clock &clock;
            std::string          ini_path;
            IniFile              ini;
            std::string          version;
            std::string          cache_dir;
            std::string          log_dir;
            std::string          last_error;
    };
}

#endif // GARLIC_CORE_CONFIGURATION_CONFIGURATION_HPP
