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
#ifndef GARLIC_CORE_CONFIGURATION_INI_FILE_HPP
#define GARLIC_CORE_CONFIGURATION_INI_FILE_HPP

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace garlic::core::configuration
{
    /**
     * Key/value pairs in the INI format of QSettings, so existing garlic-player.ini files stay readable and writable.
     *
     * Keys are written like in QSettings: "uuid" is in [General], "httpd/port" is "port" in [httpd]. Keys are sorted,
     * values are escaped and quoted the way QSettings does it.
     */
    class IniFile
    {
        public:
            static IniFile parse(std::string_view text);
            std::string    serialize() const;

            std::optional<std::string> value(std::string_view key) const;
            void                       setValue(std::string key, std::string value);
            void                       remove(std::string_view key);

            const std::map<std::string, std::string, std::less<>> &values() const noexcept { return entries; }

        private:
            std::map<std::string, std::string, std::less<>> entries;
    };
}

#endif // GARLIC_CORE_CONFIGURATION_INI_FILE_HPP
