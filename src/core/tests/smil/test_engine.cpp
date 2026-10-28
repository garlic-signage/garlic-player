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
#include "time_zone.hpp"

using namespace garlic::core::smil;
using namespace garlic::core::tests;
using namespace std::chrono_literals;

TEST_CASE("media in the body play one after another")
{
    auto runner = EngineRunner::body(R"(<img xml:id="a" src="a.jpg" dur="2s"/><img xml:id="b" src="b.jpg" dur="3s"/>)");
    CHECK(runner.run(10s) == "0s start a | 2s stop a | 2s start b | 5s stop b");
}

TEST_CASE("images without a dur that ends them are skipped, 100 ms each like garlic-lib")
{
    auto runner = EngineRunner::body(R"(
        <img xml:id="no_dur" src="a.jpg"/>
        <img xml:id="zero" src="a.jpg" dur="0s"/>
        <img xml:id="media" src="a.jpg" dur="media"/>
        <img xml:id="ok" src="a.jpg" dur="1s"/>)");
    CHECK(runner.run(10s) == "0.300s start ok | 1.300s stop ok");
}

TEST_CASE("indefinite image stays")
{
    auto runner = EngineRunner::body(R"(<img xml:id="a" src="a.jpg" dur="indefinite"/><img xml:id="b" src="b.jpg" dur="1s"/>)");
    CHECK(runner.run(1h) == "0s start a");
    CHECK_FALSE(runner.engine->nextDeadline().has_value()); // nothing to do: the host may sleep
}

TEST_CASE("video: without dur until the player reports the end, with dur the dur decides")
{
    auto runner = EngineRunner::body(R"(
        <video xml:id="own_length" src="v.mp4"/>
        <video xml:id="shorter_dur" src="v.mp4" dur="2s"/>
        <video xml:id="longer_dur" src="v.mp4" dur="8s"/>)");
    runner.media_length["v.mp4"] = 5s;
    CHECK(runner.run(30s) == "0s start own_length | 5s stop own_length"
                             " | 5s start shorter_dur | 7s stop shorter_dur"
                             " | 7s start longer_dur | 15s stop longer_dur");
}

TEST_CASE("media not in the cache are skipped, prefetch only downloads")
{
    auto runner = EngineRunner::body(R"(
        <prefetch src="next.mp4"/>
        <img xml:id="missing" src="missing.jpg" dur="2s"/>
        <img xml:id="a" src="a.jpg" dur="1s"/>)");
    runner.media.missing.insert("missing.jpg");
    CHECK(runner.run(10s) == "0.200s start a | 1.200s stop a");
    CHECK(runner.media.prefetched == std::vector<std::string>{"next.mp4"});
}

TEST_CASE("a medium the player cannot play: the dur still decides, no play log")
{
    auto runner = EngineRunner::body(R"(
        <audio xml:id="broken" src="broken.mp3" dur="4s"><param name="logContentId" value="x"/></audio>
        <video xml:id="broken_video" src="broken.mp4"/>
        <img xml:id="a" src="a.jpg" dur="1s"/>)");
    runner.failing = {"broken.mp3", "broken.mp4"};
    CHECK(runner.run(10s) == "0s start broken | 4s stop broken | 4s start broken_video | 4s stop broken_video"
                             " | 4s start a | 5s stop a");
    CHECK(runner.playLog().empty());
}

TEST_CASE("repeatCount: an image stays, a video starts again")
{
    auto runner = EngineRunner::body(R"(
        <img xml:id="a" src="a.jpg" dur="1s" repeatCount="3"/>
        <video xml:id="v" src="v.mp4" repeatCount="2"/>)");
    runner.media_length["v.mp4"] = 2s;
    CHECK(runner.run(10s) == "0s start a | 1s repeat a | 2s repeat a | 3s stop a"
                             " | 3s start v | 5s repeat v | 7s stop v");
}

TEST_CASE("body with repeatCount")
{
    auto runner = EngineRunner::body(R"(<img xml:id="a" src="a.jpg" dur="1s"/>)");
    runner.document.body.timing.repeat_count = 2;
    CHECK(runner.run(10s) == "0s start a | 1s stop a | 1s start a | 2s stop a");
}

TEST_CASE("repeating body that plays nothing tries again every 100 ms, so it starts when the download is done")
{
    EngineRunner runner(R"(<smil><body repeatCount="indefinite"><img xml:id="a" src="a.jpg" dur="2s"/></body></smil>)");
    runner.media.missing.insert("a.jpg");
    CHECK(runner.run(2500ms).empty());
    CHECK(runner.engine->nextDeadline() == runner.clock.monotonic() + 100ms);

    runner.media.missing.clear(); // the download is done
    CHECK(runner.runFor(5s) == "2.600s start a | 4.600s stop a | 4.600s start a");
}

TEST_CASE("regions: by regionName, all with the same name, by id, unknown is the first")
{
    auto runner = EngineRunner::body(R"(
        <img xml:id="by_name" src="a.jpg" region="left" dur="1s"/>
        <img xml:id="both" src="a.jpg" region="double" dur="1s"/>
        <img xml:id="by_id" src="a.jpg" region="r4" dur="1s"/>
        <img xml:id="unknown" src="a.jpg" region="nowhere" dur="1s"/>)",
                                     R"(<layout>
        <region xml:id="r1" regionName="left" z-index="1"/>
        <region xml:id="r2" regionName="double" z-index="2"/>
        <region xml:id="r3" regionName="double" z-index="3"/>
        <region xml:id="r4" z-index="0"/>
    </layout>)");
    runner.run(10s);

    // head.regions sorted by z-index: r4, r1, r2, r3
    REQUIRE(runner.starts.size() == 4);
    CHECK(runner.starts[0].regions == std::vector<std::size_t>{1});
    CHECK(runner.starts[1].regions == std::vector<std::size_t>{2, 3});
    CHECK(runner.starts[2].regions == std::vector<std::size_t>{0});
    CHECK(runner.starts[3].regions == std::vector<std::size_t>{0});
}

TEST_CASE("source: cache path, widget with query, brush without")
{
    auto runner = EngineRunner::body(R"(
        <img xml:id="a" src="a.jpg" dur="1s"/>
        <ref xml:id="w" src="clock.wgt" type="application/widget" dur="1s"><param name="city" value="Köln"/></ref>
        <brush xml:id="b" color="red" dur="1s"/>)");
    runner.run(10s);

    REQUIRE(runner.starts.size() == 3);
    CHECK(runner.starts[0].source == "cache/a.jpg");
    CHECK(runner.starts[1].source == "cache/clock.wgt?city=K%C3%B6ln");
    CHECK(runner.starts[2].source.empty());
    CHECK(runner.starts[2].media->color == "red");
}

TEST_CASE("play log: one entry per instance with logContentId, from start to stop")
{
    TimeZone tz("UTC");
    auto runner = EngineRunner::body(R"(
        <img xml:id="a" src="a.jpg" dur="2s"><param name="logContentId" value="content-a"/></img>
        <img xml:id="b" src="b.jpg" dur="1s"/>)");
    runner.run(10s);
    CHECK(runner.playLog() == "<contentPlayed><contentId>content-a</contentId><startTime>2026-01-01T00:00:00Z"
                              "</startTime><endTime>2026-01-01T00:00:02Z</endTime></contentPlayed>\n");
}

TEST_CASE("stop: stops what is playing, writes its play log, nothing follows")
{
    TimeZone tz("UTC");
    auto runner = EngineRunner::body(R"(
        <img xml:id="a" src="a.jpg" dur="10s"><param name="logContentId" value="content-a"/></img>
        <img xml:id="b" src="b.jpg" dur="1s"/>)");
    CHECK(runner.run(3s) == "0s start a");
    runner.engine->stop();
    CHECK(runner.runFor(20s) == "0s start a | 3s stop a");
    CHECK(runner.playLog().find("<endTime>2026-01-01T00:00:03Z</endTime>") != std::string::npos);
    CHECK_FALSE(runner.engine->nextDeadline().has_value());
}

TEST_CASE("body/simple.smil: the sequence from its comment")
{
    EngineRunner runner(readFile(std::filesystem::path(GARLIC_CORE_TEST_DATA) / "body/simple.smil"));
    runner.media_length["../../videos/0001.mp4"] = 10s;
    runner.failing.insert("../../audio/not_exists.mp3"); // "4s nothing"

    // begin, 1, svg-logo, 5sec. video, 2, 4sec google.com, 3, 4s nothing, 4, end
    CHECK(runner.run(30s) == "0s start img_begin | 2s stop img_begin | 2s start svg0001 | 3s stop svg0001"
                             " | 3s start img0001 | 4s stop img0001 | 4s start video_star | 9s stop video_star"
                             " | 9s start img002 | 10s stop img002 | 10s start website | 14s stop website"
                             " | 14s start img0003 | 15s stop img0003 | 15s start nix_audio | 19s stop nix_audio"
                             " | 19s start img0004 | 20s stop img0004 | 20s start img_end | 21s stop img_end");
}
