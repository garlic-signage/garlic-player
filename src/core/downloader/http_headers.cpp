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
#include "downloader/http_transport.hpp"

#include <algorithm>

namespace garlic::core::downloader
{
    namespace
    {
        // ASCII only: header names are ASCII, std::tolower would depend on the locale
        char toLowerAscii(char c) noexcept
        {
            return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
        }

        bool equalsIgnoreCase(std::string_view a, std::string_view b) noexcept
        {
            return std::ranges::equal(a, b, [](char x, char y) { return toLowerAscii(x) == toLowerAscii(y); });
        }
    }

    void HttpHeaders::add(std::string name, std::string value)
    {
        headers.push_back({std::move(name), std::move(value)});
    }

    std::optional<std::string_view> HttpHeaders::find(std::string_view name) const
    {
        auto it = std::ranges::find_if(headers, [name](const HttpHeader &h) { return equalsIgnoreCase(h.name, name); });
        if (it == headers.end())
            return std::nullopt;

        return it->value;
    }
}
