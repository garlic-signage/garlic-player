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
#include "engine_runner.hpp"

using namespace garlic::core::smil;
using namespace garlic::core::tests;
using namespace std::chrono_literals;

TEST_CASE("seq: children one after another, nested, with repeatCount")
{
    auto runner = EngineRunner::body(R"(
        <img xml:id="a" src="a.jpg" dur="1s"/>
        <seq repeatCount="2">
            <img xml:id="b" src="b.jpg" dur="1s"/>
            <seq><img xml:id="c" src="c.jpg" dur="2s"/></seq>
        </seq>
        <img xml:id="d" src="d.jpg" dur="1s"/>)");
    CHECK(runner.run(20s) == "0s start a | 1s stop a | 1s start b | 2s stop b | 2s start c | 4s stop c"
                             " | 4s start b | 5s stop b | 5s start c | 7s stop c | 7s start d | 8s stop d");
}

TEST_CASE("par: all children at once, ends with the last (endsync last)")
{
    auto runner = EngineRunner::body(R"(
        <par>
            <img xml:id="a" src="a.jpg" dur="3s"/>
            <img xml:id="b" src="b.jpg" dur="1s"/>
        </par>
        <img xml:id="c" src="c.jpg" dur="1s"/>)");
    CHECK(runner.run(20s) == "0s start a | 0s start b | 1s stop b | 3s stop a | 3s start c | 4s stop c");
}

TEST_CASE("par: endsync first stops the others, endsync id waits for that child")
{
    auto first = EngineRunner::body(R"(
        <par endsync="first">
            <img xml:id="long" src="a.jpg" dur="10s"/>
            <img xml:id="short" src="b.jpg" dur="2s"/>
        </par>
        <img xml:id="next" src="c.jpg" dur="1s"/>)");
    CHECK(first.run(20s) == "0s start long | 0s start short | 2s stop short | 2s stop long | 2s start next | 3s stop next");

    auto by_id = EngineRunner::body(R"(
        <par endsync="middle">
            <img xml:id="long" src="a.jpg" dur="10s"/>
            <img xml:id="middle" src="b.jpg" dur="5s"/>
            <img xml:id="short" src="c.jpg" dur="2s"/>
        </par>)");
    CHECK(by_id.run(20s) == "0s start long | 0s start middle | 0s start short | 2s stop short"
                            " | 5s stop middle | 5s stop long");
}

TEST_CASE("par: a skipped child ends after 100 ms and counts as first like garlic-lib")
{
    auto runner = EngineRunner::body(R"(
        <par endsync="first">
            <img xml:id="missing" src="missing.jpg" dur="1s"/>
            <img xml:id="a" src="a.jpg" dur="3s"/>
        </par>)");
    runner.media.missing.insert("missing.jpg");
    CHECK(runner.run(10s) == "0s start a | 0.100s stop a");
}

TEST_CASE("container dur: shorter cuts the children, longer leaves the rest empty, endsync is ignored")
{
    auto shorter = EngineRunner::body(R"(<seq dur="3s"><img xml:id="a" src="a.jpg" dur="2s"/><img xml:id="b" src="b.jpg" dur="2s"/></seq>
                                        <img xml:id="c" src="c.jpg" dur="1s"/>)");
    CHECK(shorter.run(10s) == "0s start a | 2s stop a | 2s start b | 3s stop b | 3s start c | 4s stop c");

    auto longer = EngineRunner::body(R"(<seq dur="5s"><img xml:id="a" src="a.jpg" dur="2s"/></seq>
                                       <img xml:id="c" src="c.jpg" dur="1s"/>)");
    CHECK(longer.run(10s) == "0s start a | 2s stop a | 5s start c | 6s stop c");

    auto with_endsync = EngineRunner::body(R"(<par dur="4s" endsync="first"><img xml:id="a" src="a.jpg" dur="1s"/>
                                             <img xml:id="b" src="b.jpg" dur="3s"/></par>)");
    CHECK(with_endsync.run(10s) == "0s start a | 0s start b | 1s stop a | 3s stop b");
}

TEST_CASE("cut by dur and repeated: the run starts clean, nothing of the old run keeps playing")
{
    auto runner = EngineRunner::body(R"(<seq dur="3s" repeatCount="2"><img xml:id="a" src="a.jpg" dur="5s"/></seq>)");
    CHECK(runner.run(10s) == "0s start a | 3s stop a | 3s start a | 6s stop a");
}

TEST_CASE("empty containers: without dur skipped, with dur a pause, dur 0 and media skipped")
{
    auto runner = EngineRunner::body(R"(
        <seq/>
        <par dur="2s"/>
        <seq dur="0s"><img xml:id="x" src="x.jpg" dur="1s"/></seq>
        <seq dur="media"><img xml:id="y" src="y.jpg" dur="1s"/></seq>
        <img xml:id="a" src="a.jpg" dur="1s"/>)");
    CHECK(runner.run(10s) == "2.300s start a | 3.300s stop a"); // 3 skipped containers, 100 ms each
}

TEST_CASE("container with only missing media: 100 ms per skipped child, repeating it tries again")
{
    auto skipped = EngineRunner::body(R"(<seq><img xml:id="m" src="m.jpg" dur="1s"/></seq><img xml:id="a" src="a.jpg" dur="1s"/>)");
    skipped.media.missing.insert("m.jpg");
    CHECK(skipped.run(10s) == "0.100s start a | 1.100s stop a");

    auto waiting = EngineRunner::body(R"(<seq repeatCount="indefinite"><img xml:id="m" src="m.jpg" dur="1s"/></seq>
                                        <img xml:id="a" src="a.jpg" dur="1s"/>)");
    waiting.media.missing.insert("m.jpg");
    CHECK(waiting.run(3500ms).empty());
    waiting.media.missing.clear();
    CHECK(waiting.runFor(5s) == "3.600s start m | 4.600s stop m | 4.600s start m");
}

TEST_CASE("pickNumber: shuffled once, then N children per run in this fixed order, round after round")
{
    auto runner = EngineRunner::body(R"(<seq repeatCount="4">
        <metadata><meta name="adapi:pickNumber" content="2"/></metadata>
        <img xml:id="a" src="a.jpg" dur="1s"/><img xml:id="b" src="b.jpg" dur="1s"/>
        <img xml:id="c" src="c.jpg" dur="1s"/><img xml:id="d" src="d.jpg" dur="1s"/>
    </seq>)");
    runner.config.random_seed = 42;
    std::string timeline      = runner.run(20s);

    REQUIRE(runner.starts.size() == 8);
    std::vector<std::string> ids;
    for (const auto &start : runner.starts)
        ids.push_back(start.media->id);

    std::set<std::string> first_round(ids.begin(), ids.begin() + 4);
    CHECK(first_round == std::set<std::string>{"a", "b", "c", "d"}); // two runs: every child once
    CHECK(std::vector(ids.begin() + 4, ids.end()) == std::vector(ids.begin(), ids.begin() + 4)); // same order again

    // the same seed gives the same order on every platform
    auto again = EngineRunner::body(R"(<seq repeatCount="4">
        <metadata><meta name="adapi:pickNumber" content="2"/></metadata>
        <img xml:id="a" src="a.jpg" dur="1s"/><img xml:id="b" src="b.jpg" dur="1s"/>
        <img xml:id="c" src="c.jpg" dur="1s"/><img xml:id="d" src="d.jpg" dur="1s"/>
    </seq>)");
    again.config.random_seed = 42;
    CHECK(again.run(20s) == timeline);
}

TEST_CASE("seq/1_simple.smil")
{
    auto runner = EngineRunner::file("seq/1_simple.smil");
    // begin, yellow, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 6, 7, 8, 9, 10, green, pink, end
    std::string expected = "0s start img_begin | 1s stop img_begin | 1s start brush_20_3 | 2s stop brush_20_3";
    int         t        = 2;
    auto        image    = [&](const std::string &id)
    {
        expected += std::format(" | {}s start {} | {}s stop {}", t, id, t + 1, id);
        ++t;
    };
    for (const char *id : {"img0001", "img0002", "img0003", "img0004", "img0005"})
        image(id);
    for (int run = 0; run < 2; ++run)
        for (const char *id : {"img0006", "img0007", "img0008", "img0009", "img0010"})
            image(id);
    for (const char *id : {"brush_35_4", "brush_36_5", "img_end"})
        image(id);
    CHECK(runner.run(60s) == expected);
}

TEST_CASE("seq/2_without_dur.smil: images without dur are skipped")
{
    auto runner = EngineRunner::file("seq/2_without_dur.smil");
    // begin, black, 10, black, 10, end; "black" are the 4 skipped images, 100 ms each
    CHECK(runner.run(20s) == "0s start img_begin | 2s stop img_begin | 2.400s start img0010 | 3.400s stop img0010"
                             " | 3.800s start img0010 | 4.800s stop img0010 | 4.800s start img_end | 6.800s stop img_end");
}

TEST_CASE("seq/3_indefinite.smil: repeats without end")
{
    auto runner   = EngineRunner::file("seq/3_indefinite.smil");
    auto timeline = runner.run(14s);
    CHECK(timeline.starts_with("0s start img_begin | 1s stop img_begin | 1s start img0001"));
    CHECK(timeline.find("7s start img_begin | 8s stop img_begin | 8s start img0001") != std::string::npos);
    CHECK(timeline.ends_with("14s start img_begin"));
}

TEST_CASE("seq/4_pick_number.smil: begin, 2 random numbers, end")
{
    auto runner = EngineRunner::file("seq/4_pick_number.smil");
    runner.run(9s);
    REQUIRE(runner.starts.size() >= 9);
    for (std::size_t run = 0; run < 2; ++run)
    {
        CHECK(runner.starts[run * 4].media->id == "img_begin");
        CHECK(runner.starts[run * 4 + 1].media->id.starts_with("img00"));
        CHECK(runner.starts[run * 4 + 2].media->id.starts_with("img00"));
        CHECK(runner.starts[run * 4 + 3].media->id == "img_end");
    }
}

TEST_CASE("par/1_simple.smil: parallel and descending")
{
    auto runner = EngineRunner::file("par/1_simple.smil");
    CHECK(runner.run(20s) == "0s start img_begin | 2s stop img_begin"
                             " | 2s start img0001 | 2s start img0002 | 2s start img0003 | 2s start img0004"
                             " | 3s stop img0004 | 4s stop img0003 | 5s stop img0002 | 6s stop img0001"
                             " | 6s start img_end | 8s stop img_end");
}

TEST_CASE("par/4_with_dur.smil: seq2 with dur 10s: 5 images, 5s black, again")
{
    auto runner   = EngineRunner::file("par/4_with_dur.smil");
    auto timeline = runner.run(21s);
    CHECK(timeline.find("4s start img_end") != std::string::npos);
    CHECK(timeline.find("5s stop img_end") != std::string::npos);
    CHECK(timeline.find("5s start img_begin") == std::string::npos); // 5 s black
    CHECK(timeline.find("10s start img_begin") != std::string::npos);
    CHECK(timeline.find("20s start img_begin") != std::string::npos);
    CHECK(timeline.find("10s start Aimg0001") != std::string::npos); // seq1 repeats on its own
}
