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

#include <algorithm>

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

// ------------------------------------------------------------------ examples of the documentation
// https://garlic-signage.com/garlic-player/docs/essentials/conditional-play/ - the expressions as written there.
// 2021-01-27 and 2022-05-11 are Wednesdays, 2022-05-16 is a Monday, 2022-05-17 a Tuesday.

namespace
{
    sys_seconds at(int y, int m, int d, int h, int minutes = 0)
    {
        return sys_days{year{y} / m / d} + hours{h} + std::chrono::minutes{minutes};
    }

    const std::string daytime = "adapi-compare(substring-after(adapi-date(), 'T'), ";

    /// The long example: 10 to 25 May 2022, …
    const std::string may_2022 = "adapi-compare(substring-before(adapi-date(), 'T'), '2022-05-10')>=0 and\n"
                                 "\tadapi-compare(substring-before(adapi-date(), 'T'), '2022-05-25')<=0 and\n";

    /// … with the weekdays Monday, Wednesday and Friday from 15:00 to 20:00
    const std::string mon_wed_fri_afternoon =
        "((1=adapi-weekday() and " + daytime + "'15:00:00')>=0 and\n\t" + daytime + "'20:00:00')<=0) or (3=adapi-weekday() and\n\t" +
        daytime + "'15:00:00')>=0 and\n\t" + daytime + "'20:00:00')<=0) or (5=adapi-weekday() and\n\t" + daytime +
        "'15:00:00')>=0 and\n\t" + daytime + "'20:00:00')<=0))";

    const std::string morning = daytime + "'08:00:00')>=0 and\n\t" + daytime + "'12:00:00')<=0";
}

TEST_CASE("docs: 3=adapi-weekday() plays on Wednesdays")
{
    TimeZone tz("UTC");
    CHECK(eval("3=adapi-weekday()", at(2021, 1, 27, 12)));
    CHECK_FALSE(eval("3=adapi-weekday()", at(2021, 1, 28, 12)));
}

TEST_CASE("docs: adapi-weekday()=1 or adapi-weekday()=5 plays on Mondays and Fridays")
{
    TimeZone tz("UTC");
    const char *expr = "adapi-weekday()=1 or adapi-weekday()=5";
    CHECK(eval(expr, at(2022, 5, 16, 12)));       // Monday: 1 (the docs say Sunday, but 0 is Sunday)
    CHECK(eval(expr, at(2022, 5, 20, 12)));       // Friday
    CHECK_FALSE(eval(expr, at(2022, 5, 15, 12))); // Sunday
}

TEST_CASE("docs: 25 to 29 January 2021 with adapi-compare and compare")
{
    TimeZone tz("UTC");
    const char *expr =
        "adapi-compare(adapi-date(), '2021-01-25T00:00:00')>0 and compare(adapi-date(), '2021-01-29T23:59:59')<=0";
    CHECK_FALSE(eval(expr, at(2021, 1, 24, 23, 59)));
    CHECK(eval(expr, at(2021, 1, 25, 0, 1)));
    CHECK(eval(expr, at(2021, 1, 27, 12)));
    CHECK(eval(expr, at(2021, 1, 29, 23, 59)));
    CHECK_FALSE(eval(expr, at(2021, 1, 30, 0, 1)));
}

TEST_CASE("docs: the long example as written never plays, 08-12 and 15-20 o'clock are joined by and")
{
    TimeZone    tz("UTC");
    std::string expr = may_2022 + "\t" + morning + " and\n\t" + mon_wed_fri_afternoon;
    for (sys_seconds time : {at(2022, 5, 16, 10), at(2022, 5, 16, 16), at(2022, 5, 17, 10), at(2022, 5, 11, 17)})
        CHECK_FALSE(eval(expr, time));
}

TEST_CASE("docs: the long example as meant, 08-12 or Monday, Wednesday, Friday 15-20 o'clock")
{
    TimeZone    tz("UTC");
    std::string expr = may_2022 + "\t(" + morning + " or\n\t" + mon_wed_fri_afternoon + ")";
    CHECK(eval(expr, at(2022, 5, 17, 10)));       // Tuesday morning
    CHECK(eval(expr, at(2022, 5, 16, 16)));       // Monday afternoon
    CHECK(eval(expr, at(2022, 5, 11, 20)));       // Wednesday 20:00
    CHECK_FALSE(eval(expr, at(2022, 5, 17, 16))); // Tuesday afternoon
    CHECK_FALSE(eval(expr, at(2022, 5, 16, 13))); // Monday between
    CHECK_FALSE(eval(expr, at(2022, 5, 9, 10)));  // before the 10th
    CHECK_FALSE(eval(expr, at(2022, 5, 26, 10))); // after the 25th
}

TEST_CASE("docs: adapi-compare with smil-playerName/smil-playerId needs =0, alone it is never true")
{
    // compare gives 0 when equal, a number is no boolean: never played, like in garlic-lib
    CHECK_FALSE(eval("adapi-compare(smil-playerName(),'Lobby')"));
    CHECK_FALSE(eval("adapi-compare(smil-playerId(),'8f1c-uuid')"));
    CHECK(eval("adapi-compare(smil-playerName(),'Lobby')=0"));
    CHECK(eval("adapi-compare(smil-playerId(),'8f1c-uuid')=0"));
    CHECK_FALSE(eval("adapi-compare(smil-playerName(),'Entrance')=0"));
}

TEST_CASE("docs: < in an expr attribute has to be &lt;, unescaped the whole index is broken")
{
    auto smil = [](std::string_view expr)
    {
        return "<smil><body><ref expr=\"" + std::string(expr) +
               "\" src=\"rss-widget.wgt\" dur=\"10s\" type=\"application/widget\"/></body></smil>";
    };
    std::string expr = may_2022 + "\t" + morning;

    CHECK_FALSE(parse(smil(expr)).has_value()); // the text of the error differs between libxml2 versions

    std::string escaped = expr;
    for (std::size_t at_lt = escaped.find('<'); at_lt != std::string::npos; at_lt = escaped.find('<', at_lt))
        escaped.replace(at_lt, 1, "&lt;");
    auto document = parse(smil(escaped));
    REQUIRE(document.has_value());
    REQUIRE(document->body.children.size() == 1);
    std::string normalized = expr; // XML turns line breaks and tabs in attribute values into spaces
    std::ranges::replace(normalized, '\n', ' ');
    std::ranges::replace(normalized, '\t', ' ');
    CHECK(std::get<Media>(document->body.children[0]).expr == normalized); // &lt; decoded again

    TimeZone tz("UTC");
    CHECK(eval(std::get<Media>(document->body.children[0]).expr, at(2022, 5, 17, 10)));
}
