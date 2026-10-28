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
#include <doctest/doctest.h>

#include "smil/parser.hpp"

#include <fstream>
#include <sstream>

using namespace garlic::core::smil;
using namespace std::chrono_literals;

namespace
{
    std::string readFile(const std::string &name)
    {
        std::ifstream      in(std::string(GARLIC_CORE_TEST_DATA) + "/" + name, std::ios::binary);
        std::ostringstream content;
        content << in.rdbuf();
        return content.str();
    }

    Head parseHead(std::string_view head)
    {
        auto document = parse("<smil><head>" + std::string(head) + "</head><body/></smil>");
        REQUIRE(document.has_value());
        return document->head;
    }
}

TEST_CASE("without head: default values of garlic-lib and the region 'screen'")
{
    auto document = parse("<smil><body/></smil>");
    REQUIRE(document.has_value());
    const Head &head = document->head;

    CHECK(head.title == "No Title");
    CHECK(head.refresh == 0s);
    CHECK(head.root_layout.width == 1366);
    CHECK(head.root_layout.height == 768);
    CHECK(head.root_layout.background_color == "black");
    REQUIRE(head.regions.size() == 1);
    CHECK(head.regions[0].id == "screen");
    CHECK(head.regions[0].width == 1);
    CHECK(head.regions[0].height == 1);
    CHECK(head.regions[0].fit == "hidden");
    CHECK(head.regions[0].background_color == "transparent");
    CHECK(head.subscriptions.empty());
    CHECK(head.placeholders.empty());
}

TEST_CASE("head_simple.smil")
{
    auto document = parse(readFile("head/head_simple.smil"));
    REQUIRE(document.has_value());
    const Head &head = document->head;

    CHECK(head.title == "Simple SMIL header for testing");
    CHECK(head.refresh == 60s);
    CHECK(head.root_layout.width == 1920);
    CHECK(head.root_layout.height == 1080);

    REQUIRE(head.regions.size() == 1);
    const Region &region = head.regions[0];
    CHECK(region.id == "region_8_1"); // no id in the SMIL: tag, line, number
    CHECK(region.region_name == "test_screen");
    CHECK(region.top == 0);
    CHECK(region.left == 0);
    CHECK(region.width == 1); // 1920 of 1920 pixels
    CHECK(region.height == 1);
    CHECK(region.z_index == 1);
    CHECK(region.background_color == "#000");
}

TEST_CASE("head_complex.smil: regions sorted by z-index, subscriptions")
{
    auto document = parse(readFile("head/head_complex.smil"));
    REQUIRE(document.has_value());
    const Head &head = document->head;

    CHECK(head.title == "Complex SMIL header for testing");
    CHECK(head.refresh == 2s); // raw value, the minimum is the business of the index manager
    CHECK(head.root_layout.background_color == "#000");

    REQUIRE(head.regions.size() == 5);
    CHECK(head.regions[0].region_name == "screen1");
    CHECK(head.regions[1].region_name == "screen2"); // same z-index: order of the SMIL
    CHECK(head.regions[2].region_name == "screen3");
    CHECK(head.regions[3].region_name == "screen4");
    CHECK(head.regions[4].region_name == "screen5");
    CHECK(head.regions[1].left == doctest::Approx(0.5));
    CHECK(head.regions[1].width == doctest::Approx(0.25));
    CHECK(head.regions[1].background_color == "green");

    REQUIRE(head.subscriptions.size() == 5);
    CHECK(head.subscriptions[0].type == "TaskSchedule");
    CHECK(head.subscriptions[0].action.empty());
    CHECK(head.subscriptions[0].method == "get");
    CHECK(head.subscriptions[2].type == "PlaylogCollection");
    CHECK(head.subscriptions[2].action == "http://logs");
    CHECK(head.subscriptions[2].method == "put");
}

TEST_CASE("regions: lower z-index first")
{
    Head head = parseHead(R"(<layout>
        <region regionName="top" z-index="2"/>
        <region regionName="bottom" z-index="0"/>
        <region regionName="middle" z-index="1"/>
    </layout>)");

    REQUIRE(head.regions.size() == 3);
    CHECK(head.regions[0].region_name == "bottom");
    CHECK(head.regions[1].region_name == "middle");
    CHECK(head.regions[2].region_name == "top");
}

TEST_CASE("region sizes: percent, pixels, px; empty values keep the default")
{
    Head head = parseHead(R"(<layout>
        <root-layout width="1920" height="1080"/>
        <region id="r" top="25%" left="960px" width="480" height="" z-index="3" fit="meet" mediaAlign="center"
                soundLevel="50%" backgroundImage="bg.jpg" backgroundRepeat="no-repeat"/>
    </layout>)");

    const Region &region = head.regions.at(0);
    CHECK(region.id == "r");
    CHECK(region.top == doctest::Approx(0.25));
    CHECK(region.left == doctest::Approx(0.5));
    CHECK(region.width == doctest::Approx(0.25));
    CHECK(region.height == 1); // empty: default (garlic-lib crashed here)
    CHECK(region.fit == "meet");
    CHECK(region.media_align == "center");
    CHECK(region.sound_level == "50%");
    CHECK(region.background_image == "bg.jpg");
    CHECK(region.background_repeat == "no-repeat");
}

TEST_CASE("region ids: xml:id before id, generated ids are unique")
{
    Head head = parseHead(R"(<layout><region xml:id="new" id="old"/><region/><region/></layout>)");

    REQUIRE(head.regions.size() == 3);
    CHECK(head.regions[0].id == "new");
    CHECK(head.regions[1].id == "region_1_1");
    CHECK(head.regions[2].id == "region_1_2");
}

TEST_CASE("root-layout: background, missing size keeps the default")
{
    Head head =
        parseHead(R"(<layout><root-layout backgroundColor="white" backgroundImage="a.png" backgroundRepeat="no"/>
                     <region left="683"/></layout>)");

    CHECK(head.root_layout.width == 1366);
    CHECK(head.root_layout.background_color == "white");
    CHECK(head.root_layout.background_image == "a.png");
    CHECK(head.root_layout.background_repeat == "no");
    CHECK(head.regions.at(0).left == doctest::Approx(0.5)); // based on the default width
}

TEST_CASE("meta: refresh needs content, unknown meta is ignored")
{
    CHECK(parseHead(R"(<meta http-equiv="Refresh"/>)").refresh == 0s);
    CHECK(parseHead(R"(<meta http-equiv="Refresh" content="abc"/>)").refresh == 0s);
    CHECK(parseHead(R"(<meta name="author" content="x"/>)").title == "No Title");
}

TEST_CASE("state: placeholders for src=\"{name}\"")
{
    Head head = parseHead(R"(<state><data>
        <logo>http://example.com/logo.png</logo>
        <empty></empty>
    </data></state>)");

    REQUIRE(head.placeholders.size() == 1);
    CHECK(head.placeholders.at("{logo}") == "http://example.com/logo.png");
}

TEST_CASE("subscriptions: refresh and retry intervals, values trimmed")
{
    Head head = parseHead(R"(<metadata><x-server xmlns="http://schemas.adfotain.org/adapi-1.0"><subscriptionList>
        <subscription>
            <type>SystemReport</type>
            <action>
                http://hub/report
            </action>
            <method>put</method>
            <refreshInterval>300</refreshInterval>
            <retryInterval>30</retryInterval>
        </subscription>
    </subscriptionList></x-server></metadata>)");

    REQUIRE(head.subscriptions.size() == 1);
    CHECK(head.subscriptions[0].action == "http://hub/report");
    CHECK(head.subscriptions[0].refresh_interval == 300);
    CHECK(head.subscriptions[0].retry_interval == 30);
}

TEST_CASE("errors: broken XML with line, wrong root element")
{
    auto broken = parse("<smil>\n<head>\n</smil>");
    REQUIRE_FALSE(broken.has_value());
    CHECK(broken.error().line == 3);
    CHECK_FALSE(broken.error().message.empty());

    auto wrong_root = parse("<html/>");
    REQUIRE_FALSE(wrong_root.has_value());
    CHECK(wrong_root.error().message == "root element is not <smil>");

    CHECK_FALSE(parse("").has_value());
}
