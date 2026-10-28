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

using namespace garlic::core::smil;
using namespace garlic::core::tests;
using namespace std::chrono_literals;

TEST_CASE("nested seq and par")
{
    Container body = parseBody(R"(
        <img xml:id="a" src="a.jpg"/>
        <seq xml:id="s1">
            <img xml:id="b" src="b.jpg"/>
            <par xml:id="p1">
                <video xml:id="c" src="c.mp4"/>
                <seq xml:id="s2"><img xml:id="d" src="d.jpg"/></seq>
            </par>
        </seq>
        <par xml:id="p2"/>
    )");

    CHECK(outline(body) == "body_1_1(a s1(b p1(c s2(d))) p2())");
    CHECK(container(body.children[1]).kind == ContainerKind::Seq);
    CHECK(container(body.children[2]).kind == ContainerKind::Par);
}

TEST_CASE("container attributes")
{
    Container body = parseBody(
        R"x(<seq xml:id="s" title="Morning" expr="adapi-compare('a','b')" dur="1h" repeatCount="3" restart="never"/>)x");

    const Container &seq = container(body.children.at(0));
    CHECK(seq.title == "Morning");
    CHECK(seq.expr == "adapi-compare('a','b')");
    CHECK(seq.timing.dur.value == 1h);
    CHECK(seq.timing.repeat_count == 3);
    CHECK(seq.timing.restart == Restart::Never);
    CHECK_FALSE(seq.pick_number.has_value());
    CHECK(seq.endsync == EndSync::Last);
}

TEST_CASE("seq: children take only the first begin value, par children the whole list")
{
    Container body = parseBody(R"(
        <seq><img xml:id="in_seq" src="a.jpg" begin="2s;3s;4s"/></seq>
        <par><img xml:id="in_par" src="a.jpg" begin="2s;3s;4s"/></par>
    )");

    const auto &in_seq = std::get<Media>(container(body.children[0]).children.at(0));
    REQUIRE(in_seq.timing.begin.size() == 1);
    CHECK(offset(in_seq.timing.begin[0]) == 2s);

    const auto &in_par = std::get<Media>(container(body.children[1]).children.at(0));
    CHECK(in_par.timing.begin.size() == 3);
}

TEST_CASE("seq: metadata as first child gives pick_number and is no child")
{
    Container body = parseBody(R"(
        <seq xml:id="pick2">
            <metadata>
                <meta name="adapi:pickingAlgorithm" content="shuffle"/>
                <meta name="adapi:pickingBehavior" content="pickN"/>
                <meta name="adapi:pickNumber" content="2"/>
            </metadata>
            <img xml:id="a" src="a.jpg"/><img xml:id="b" src="b.jpg"/><img xml:id="c" src="c.jpg"/>
        </seq>
        <seq xml:id="empty_metadata"><metadata/><img xml:id="d" src="d.jpg"/></seq>
        <seq xml:id="zero"><metadata><meta name="adapi:pickNumber" content="0"/></metadata></seq>
        <seq xml:id="late"><img xml:id="e" src="e.jpg"/><metadata/></seq>
    )");

    CHECK(outline(body) == "body_1_1(pick2(a b c) empty_metadata(d) zero() late(e))");
    CHECK(container(body.children[0]).pick_number == 2);
    CHECK(container(body.children[1]).pick_number == 1); // empty metadata: shuffle with 1 like garlic-lib
    CHECK(container(body.children[2]).pick_number == 1); // 0 would play nothing
    CHECK_FALSE(container(body.children[3]).pick_number.has_value()); // only as first child
}

TEST_CASE("par: endsync")
{
    Container body = parseBody(R"(
        <par xml:id="none"/>
        <par xml:id="last" endsync="last"/>
        <par xml:id="first" endsync="first"/>
        <par xml:id="by_id" endsync="seq3"/>
        <seq xml:id="seq" endsync="first"/>
    )");

    CHECK(container(body.children[0]).endsync == EndSync::Last);
    CHECK(container(body.children[1]).endsync == EndSync::Last);
    CHECK(container(body.children[2]).endsync == EndSync::First);
    CHECK(container(body.children[3]).endsync == EndSync::Element);
    CHECK(container(body.children[3]).endsync_id == "seq3");
    CHECK(container(body.children[4]).endsync == EndSync::Last); // only par
}

TEST_CASE("seq/1_simple.smil")
{
    Document document = parseFile("seq/1_simple.smil");
    CHECK(outline(document.body) ==
          "body_10_2(img_begin brush_20_3 seq1(img0001 img0002 img0003 img0004 img0005) "
          "seq2(img0006 img0007 img0008 img0009 img0010) brush_35_4 brush_36_5 img_end)"); // _2: the region of the head is _1
    CHECK(container(document.body.children[3]).timing.repeat_count == 2);
}

TEST_CASE("seq/4_pick_number.smil")
{
    Document document = parseFile("seq/4_pick_number.smil");
    const Container &seq1 = container(document.body.children.at(0));
    CHECK(seq1.timing.repeat_indefinite);
    const Container &picker = container(seq1.children.at(1));
    CHECK(picker.pick_number == 2);
    CHECK(picker.children.size() == 10);
}

TEST_CASE("seq/5_media_repeats.smil: begin lists in a seq")
{
    Document         document = parseFile("seq/5_media_repeats.smil");
    const Container &seq      = container(document.body.children.at(0));

    const auto &img0001 = std::get<Media>(seq.children.at(1));
    REQUIRE(img0001.timing.begin.size() == 1); // "2s;3s;4s;5s;": only the first in a seq
    CHECK(offset(img0001.timing.begin[0]) == 2s);
    CHECK(img0001.timing.repeat_count == 5);

    const auto &video = std::get<Media>(seq.children.at(4));
    CHECK(video.kind == MediaKind::Video);
    CHECK(video.timing.dur.type == Duration::Type::None);
    CHECK(video.timing.repeat_count == 3);
}

TEST_CASE("par/6_endsync.smil")
{
    Document         document = parseFile("par/6_endsync.smil");
    const Container &global   = container(document.body.children.at(0));
    REQUIRE(global.children.size() == 4);

    CHECK(container(global.children[1]).endsync == EndSync::Last);
    CHECK(container(global.children[2]).endsync == EndSync::First);
    CHECK(container(global.children[3]).endsync == EndSync::Element);
    CHECK(container(global.children[3]).endsync_id == "seq3");
    CHECK(outline(global.children[3]) == "par_id(imgDbegin seq3(imgD0001 imgD0002 imgDB003 imgD0004) imgDend)");
}

TEST_CASE("all seq and par test files can be parsed")
{
    for (const char *directory : {"seq", "par"})
    {
        for (const auto &entry : std::filesystem::directory_iterator(std::filesystem::path(GARLIC_CORE_TEST_DATA) /
                                                                     directory))
        {
            CAPTURE(entry.path().string());
            auto document = parse(readFile(entry.path()));
            CHECK(document.has_value());
            if (document)
                CHECK_FALSE(document->body.children.empty());
        }
    }
}
