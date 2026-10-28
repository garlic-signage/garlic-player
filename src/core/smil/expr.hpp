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
#ifndef GARLIC_CORE_SMIL_EXPR_HPP
#define GARLIC_CORE_SMIL_EXPR_HPP

#include <chrono>
#include <string>
#include <string_view>

namespace garlic::core::smil
{
    struct ExprContext
    {
        std::chrono::system_clock::time_point now;
        std::string                           player_id;   // smil-playerId()
        std::string                           player_name; // smil-playerName()
    };

    /**
     * Whether an element with this expr attribute is played, garlic-lib: Expr and AdapiWrapper.
     * XPath 1.0 (libxml2) with the functions adapi-date, adapi-gmdate, adapi-weekday, adapi-gmweekday,
     * adapi-compare, compare, smil-playerId and smil-playerName. An empty expr is true; errors, unknown functions
     * and results that are not boolean true are false.
     */
    bool evaluateExpr(std::string_view expr, const ExprContext &context);
}

#endif // GARLIC_CORE_SMIL_EXPR_HPP
