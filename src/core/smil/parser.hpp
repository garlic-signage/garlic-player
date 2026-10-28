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
#ifndef GARLIC_CORE_SMIL_PARSER_HPP
#define GARLIC_CORE_SMIL_PARSER_HPP

#include "smil/document.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace garlic::core::smil
{
    struct ParseError
    {
        int         line = 0; // 0 if unknown
        std::string message;  // for logging only
    };

    /**
     * Reads a SMIL document into plain data. No timers, no downloads, no side effects: what happens with the data is
     * decided by the caller. Fails only if the XML is broken or the root element is not <smil>.
     */
    std::expected<Document, ParseError> parse(std::string_view xml);
}

#endif // GARLIC_CORE_SMIL_PARSER_HPP
