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
#include "helpers.hpp"
#include "smil/expr.hpp"
#include "time_zone.hpp"

using namespace garlic::core::smil;
using namespace garlic::core::tests;
using namespace std::chrono;

namespace
{
    // Wednesday, 2026-10-28 07:00 UTC = 08:00 in CET
    const sys_seconds wednesday = sys_days{October / 28 / 2026} + 7h;

    bool eval(std::string_view expr, sys_seconds now = wednesday)
    {
        return evaluateExpr(expr, ExprContext{now, "8f1c-uuid", "Lobby"});
    }
}

TEST_CASE("empty expr: play")
{
    CHECK(eval(""));
    CHECK(eval("  "));
}

TEST_CASE("adapi-date and adapi-gmdate")
{
    TimeZone tz("CET-1");
    CHECK(eval("adapi-date() = '2026-10-28T08:00:00'"));
    CHECK(eval("adapi-gmdate() = '2026-10-28T07:00:00Z'"));
    CHECK(eval("substring-before(adapi-date(), 'T') = '2026-10-28'"));
}

TEST_CASE("adapi-weekday and adapi-gmweekday: 0 = Sunday")
{
    TimeZone tz("CET-1");
    CHECK(eval("adapi-weekday() = 3"));
    CHECK(eval("adapi-gmweekday() = 3"));

    auto late = sys_days{October / 28 / 2026} + 23h + 30min; // Thursday 00:30 in CET, Wednesday in UTC
    CHECK(eval("adapi-weekday() = 4", late));
    CHECK(eval("adapi-gmweekday() = 3", late)); // garlic-lib returned the local weekday here
}

TEST_CASE("compare and adapi-compare")
{
    CHECK(eval("compare('a', 'b') = -1"));
    CHECK(eval("compare('b', 'b') = 0"));
    CHECK(eval("adapi-compare('2026-10-28', '2026-01-01') = 1"));
    CHECK(eval("compare('ä', 'z') = 1")); // code points: ä after z
}

TEST_CASE("smil-playerId and smil-playerName")
{
    CHECK(eval("smil-playerId() = '8f1c-uuid'")); // garlic-lib: always false
    CHECK(eval("smil-playerName() = 'Lobby'"));
    CHECK_FALSE(eval("smil-playerName() = 'Kitchen'"));
}

TEST_CASE("errors, unknown functions and non-boolean results are false")
{
    CHECK_FALSE(eval("adapi-compare("));
    CHECK_FALSE(eval("unknown-function()"));
    CHECK_FALSE(eval("adapi-date(1)"));
    CHECK_FALSE(eval("1"));
    CHECK_FALSE(eval("'true'"));
    CHECK(eval("true()"));
}

TEST_CASE("seq/6_conditional_play.smil: date range of January 2022")
{
    TimeZone  tz("UTC");
    Document  document = parseFile("seq/6_conditional_play.smil");
    std::string expr;
    for (const Node &node : document.body.children)
        if (const auto *c = std::get_if<Container>(&node); c && !c->expr.empty())
            expr = c->expr;
    REQUIRE_FALSE(expr.empty());
    CHECK(expr.contains(">=")); // &gt; already decoded by the XML parser

    CHECK(eval(expr, sys_days{January / 15 / 2022} + 12h));
    CHECK_FALSE(eval(expr, sys_days{February / 1 / 2022}));
    CHECK_FALSE(eval(expr, wednesday));
}
