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

using namespace garlic::core::smil;
using namespace std::chrono_literals;

namespace
{
    Container parseBody(std::string_view body, std::string_view head = "")
    {
        auto document =
            parse("<smil><head>" + std::string(head) + "</head><body>" + std::string(body) + "</body></smil>");
        REQUIRE(document.has_value());
        return document->body;
    }

    Media parseOne(std::string_view element)
    {
        Container body = parseBody(element);
        REQUIRE(body.children.size() == 1);
        return std::get<Media>(body.children[0]);
    }

    std::chrono::milliseconds offset(const TimeCondition &condition)
    {
        REQUIRE(std::holds_alternative<Offset>(condition));
        return std::get<Offset>(condition).value;
    }

    Duration dur(std::string_view value)
    {
        return parseOne(R"(<img src="a.jpg" dur=")" + std::string(value) + R"("/>)").timing.dur;
    }
}

TEST_CASE("media kinds like the ElementFactory of garlic-lib, unknown elements are left out")
{
    Container body = parseBody(R"(
        <img src="a.jpg"/>
        <video src="a.mp4"/>
        <audio src="a.mp3"/>
        <text src="a.html"/>
        <brush color="red"/>
        <prefetch src="b.mp4"/>
        <ref src="a.png" type="image/png"/>
        <ref src="a.webm" type="video/webm"/>
        <ref src="a.ogg" type="audio/ogg"/>
        <ref src="a.html" type="text/html"/>
        <ref src="a.wgt" type="application/widget"/>
        <ref src="adapi:blankScreen"/>
        <ref src="a.xyz" type="model/unknown"/>
        <ref src="a.xyz"/>
        <animate/>
    )");

    std::vector<MediaKind> kinds;
    for (const Node &node : body.children)
        kinds.push_back(std::get<Media>(node).kind);

    CHECK(kinds == std::vector{MediaKind::Image, MediaKind::Video, MediaKind::Audio, MediaKind::Web,
                               MediaKind::Brush, MediaKind::Prefetch, MediaKind::Image, MediaKind::Video,
                               MediaKind::Audio, MediaKind::Web, MediaKind::Widget, MediaKind::Command});
}

TEST_CASE("media attributes")
{
    Media media = parseOne(R"x(<img xml:id="img1" title="Logo" src="a.jpg" region="screen1" fit="meetBest"
                                   mediaAlign="center" expr="adapi-compare('a','b')" soundLevel="50%"/>)x");

    CHECK(media.id == "img1");
    CHECK(media.title == "Logo");
    CHECK(media.src == "a.jpg");
    CHECK(media.region == "screen1");
    CHECK(media.fit == "meetBest");
    CHECK(media.media_align == "center");
    CHECK(media.expr == "adapi-compare('a','b')");
    CHECK(media.sound_level.empty()); // only audio and video
    CHECK(media.query.empty());
}

TEST_CASE("fit and mediaAlign empty: the region decides")
{
    Media media = parseOne(R"(<img src="a.jpg"/>)");
    CHECK(media.fit.empty());
    CHECK(media.media_align.empty());
    CHECK(media.id == "img_1_2"); // body_1_1 is the body
}

TEST_CASE("soundLevel for audio and video, color for brush")
{
    Container body = parseBody(R"(<video src="a.mp4" soundLevel="30%"/><audio src="a.mp3" soundLevel="0%"/>
                                  <brush color="#ff0000" src="ignored"/>)");

    CHECK(std::get<Media>(body.children[0]).sound_level == "30%");
    CHECK(std::get<Media>(body.children[1]).sound_level == "0%");
    const auto &brush = std::get<Media>(body.children[2]);
    CHECK(brush.color == "#ff0000");
    CHECK(brush.src.empty());
}

TEST_CASE("params: logContentId, filename, cacheControl")
{
    Media media = parseOne(R"(<video src="a.mp4">
        <param name="logContentId" value="content-42"/>
        <param name="filename" value="intro.mp4"/>
        <param name="cacheControl" value="onlyifcached"/>
        <param name="other" value="x"/>
        <param name="no-value"/>
    </video>)");

    CHECK(media.log_content_id == "content-42");
    CHECK(media.filename == "intro.mp4");
    CHECK(media.cache_control == CacheControl::OnlyIfCached);
    CHECK(media.query.empty()); // only widgets get a query
}

TEST_CASE("widget: the other params become a percent-encoded query")
{
    Media media = parseOne(R"(<ref src="clock.wgt" type="application/widget">
        <param name="logContentId" value="w1"/>
        <param name="city" value="Köln Süd"/>
        <param name="format" value="a&amp;b=c"/>
    </ref>)");

    CHECK(media.log_content_id == "w1");
    CHECK(media.query == "?city=K%C3%B6ln%20S%C3%BCd&format=a%26b%3Dc");
}

TEST_CASE("src with placeholder from the head")
{
    Container body = parseBody(R"(<img src="{logo}"/><img src="{unknown}"/><img src=""/>)",
                               "<state><data><logo>http://example.com/logo.png</logo></data></state>");

    CHECK(std::get<Media>(body.children[0]).src == "http://example.com/logo.png");
    CHECK(std::get<Media>(body.children[1]).src.empty()); // unknown placeholder: empty like in garlic-lib
    CHECK(std::get<Media>(body.children[2]).src.empty()); // garlic-lib crashed on an empty src
}

TEST_CASE("dur: clock values")
{
    CHECK(dur("10s").type == Duration::Type::Clock);
    CHECK(dur("10s").value == 10s);
    CHECK(dur("10").value == 10s);
    CHECK(dur("2.5s").value == 2500ms);
    CHECK(dur("500ms").value == 500ms); // garlic-lib: 0
    CHECK(dur("1.5min").value == 90s);
    CHECK(dur("2h").value == 2h);
    CHECK(dur("1:30").value == 90s);
    CHECK(dur("01:30:00.5").value == 5400500ms);
    CHECK(dur("30d").value == 0ms); // unknown unit
    CHECK(dur("-5s").value == 0ms); // negative durations are 0
    CHECK(dur("720h").value == 720h); // 30 days, garlic-lib overflowed at about 24 days
    CHECK(dur("´1s").value == 1s);    // like garlic-lib: one leading character that is no digit is dropped
    CHECK(dur(".5s").value == 500ms);
}

TEST_CASE("dur: missing, indefinite, media")
{
    CHECK(parseOne(R"(<img src="a.jpg"/>)").timing.dur.type == Duration::Type::None);
    CHECK(dur("").type == Duration::Type::Indefinite);
    CHECK(dur("indefinite").type == Duration::Type::Indefinite);
    CHECK(dur("media").type == Duration::Type::Media);
}

TEST_CASE("begin: default 0s, offsets, lists")
{
    Timing timing = parseOne(R"(<img src="a.jpg"/>)").timing;
    REQUIRE(timing.begin.size() == 1);
    CHECK(offset(timing.begin[0]) == 0ms);
    CHECK(timing.end.empty());

    timing = parseOne(R"(<img src="a.jpg" begin="5s; -2s ;" end="00:01:00"/>)").timing;
    REQUIRE(timing.begin.size() == 2);
    CHECK(offset(timing.begin[0]) == 5s);
    CHECK(offset(timing.begin[1]) == -2s);
    REQUIRE(timing.end.size() == 1);
    CHECK(offset(timing.end[0]) == 60s);
}

TEST_CASE("begin: indefinite only alone, unreadable values give the default")
{
    Timing timing = parseOne(R"(<img src="a.jpg" begin="indefinite"/>)").timing;
    REQUIRE(timing.begin.size() == 1);
    CHECK(std::holds_alternative<Indefinite>(timing.begin[0]));

    timing = parseOne(R"(<img src="a.jpg" begin="indefinite;5s"/>)").timing;
    REQUIRE(timing.begin.size() == 1);
    CHECK(offset(timing.begin[0]) == 5s);

    timing = parseOne(R"(<img src="a.jpg" begin="garbage"/>)").timing;
    REQUIRE(timing.begin.size() == 1);
    CHECK(offset(timing.begin[0]) == 0ms);
}

TEST_CASE("repeatCount and restart")
{
    Timing timing = parseOne(R"(<img src="a.jpg"/>)").timing;
    CHECK(timing.repeat_count == 1);
    CHECK_FALSE(timing.repeat_indefinite);
    CHECK(timing.restart == Restart::Always);

    CHECK(parseOne(R"(<img src="a.jpg" repeatCount="3"/>)").timing.repeat_count == 3);
    CHECK(parseOne(R"(<img src="a.jpg" repeatCount="2.5"/>)").timing.repeat_count == 2);
    CHECK(parseOne(R"(<img src="a.jpg" repeatCount="0"/>)").timing.repeat_count == 1);
    CHECK(parseOne(R"(<img src="a.jpg" repeatCount="indefinite"/>)").timing.repeat_indefinite);

    CHECK(parseOne(R"(<img src="a.jpg" restart="never"/>)").timing.restart == Restart::Never);
    CHECK(parseOne(R"(<img src="a.jpg" restart="whenNotActive"/>)").timing.restart == Restart::WhenNotActive);
    CHECK(parseOne(R"(<img src="a.jpg" restart="always"/>)").timing.restart == Restart::Always);
}

TEST_CASE("body: container with id and timing, missing body is empty")
{
    auto document = parse(R"(<smil><body xml:id="main" repeatCount="indefinite"><img src="a.jpg"/></body></smil>)");
    REQUIRE(document.has_value());
    CHECK(document->body.kind == ContainerKind::Body);
    CHECK(document->body.id == "main");
    CHECK(document->body.timing.repeat_indefinite);
    CHECK(document->body.children.size() == 1);

    auto without_body = parse("<smil><head/></smil>");
    REQUIRE(without_body.has_value());
    CHECK(without_body->body.children.empty());
}
