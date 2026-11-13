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
#include "common/text.hpp"

#if !(defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L)
#include <locale>
#include <sstream>
#include <system_error>
#endif

namespace garlic::core::common
{
    std::string toLower(std::string_view text)
    {
        std::string result(text);
        for (char &c : result)
        {
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a');
        }
        return result;
    }

    std::from_chars_result fromChars(const char *first, const char *last, double &value)
    {
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
        return std::from_chars(first, last, value);
#else
        // from_chars reads no leading white space and no '+', the stream would
        if (first == last || *first == '+' || *first == ' ' || *first == '\t' || *first == '\n' || *first == '\r')
            return {first, std::errc::invalid_argument};
        std::istringstream in(std::string(first, last));
        in.imbue(std::locale::classic());
        double read = 0;
        in >> read;
        if (in.fail())
            return {first, std::errc::invalid_argument};
        auto consumed = in.eof() ? static_cast<std::ptrdiff_t>(last - first) : static_cast<std::ptrdiff_t>(in.tellg());
        value         = read;
        return {first + consumed, std::errc()};
#endif
    }
}
