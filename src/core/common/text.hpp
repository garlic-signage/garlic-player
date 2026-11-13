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
#ifndef GARLIC_CORE_COMMON_TEXT_HPP
#define GARLIC_CORE_COMMON_TEXT_HPP

#include <charconv>
#include <string>
#include <string_view>

namespace garlic::core::common
{
    /// Only A-Z become a-z, independent of the locale (header names, file extensions, MIME types).
    std::string toLower(std::string_view text);

    /**
     * std::from_chars for double, independent of the locale. Where the standard library has no floating point
     * from_chars (libc++ before 20, e.g. NDK 26 of Qt 6.8 for Android) a stream with the classic locale reads it.
     */
    std::from_chars_result fromChars(const char *first, const char *last, double &value);
}

#endif // GARLIC_CORE_COMMON_TEXT_HPP
