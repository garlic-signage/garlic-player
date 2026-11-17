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
#include "file_url.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string_view>

namespace player
{
    std::string fileUrl(const std::string &source)
    {
        // a URL with a scheme stays as it is: http://, file://, also data:,… and about:blank without "//"; at least
        // two letters before ":", so a Windows path like C:\… is a path
        std::size_t colon = source.find(':');
        if (colon >= 2 && colon != std::string::npos &&
            std::all_of(source.begin(), source.begin() + static_cast<std::ptrdiff_t>(colon),
                        [](unsigned char c) { return std::isalnum(c) != 0 || c == '+' || c == '-' || c == '.'; }) &&
            std::isalpha(static_cast<unsigned char>(source.front())) != 0)
            return source;
        std::size_t query = source.find_first_of("?#");
        std::string path  = source.substr(0, query);
        std::string url   = "file://";
        for (unsigned char c : path)
        {
            if (std::isalnum(c) != 0 || std::string_view("/-._~").find(static_cast<char>(c)) != std::string_view::npos)
                url += static_cast<char>(c);
            else
            {
                constexpr std::string_view digits = "0123456789ABCDEF";
                url += '%';
                url += digits[c >> 4U];
                url += digits[c & 0xFU];
            }
        }
        return query == std::string::npos ? url : url + source.substr(query);
    }
}
