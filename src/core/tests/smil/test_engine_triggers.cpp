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

namespace
{
    std::optional<InstanceId> lastInstance(const EngineRunner &runner, const std::string &id)
    {
        std::optional<InstanceId> found;
        for (const StartMedia &start : runner.starts)
            if (start.media->id == id)
                found = start.id;
        return found;
    }
}

TEST_CASE("syncbase: begin and end of another element, with offset")
{
    auto runner = EngineRunner::body(R"x(<par>
        <img xml:id="a" src="a.jpg" dur="3s"/>
        <img xml:id="with_a" src="b.jpg" begin="a.begin" end="a.end"/>
        <img xml:id="after_a" src="c.jpg" begin="a.end+1s" dur="1s"/>
    </par>)x");
    CHECK(runner.run(10s) == "0s start a | 0s start with_a | 3s stop a | 3s stop with_a | 4s start after_a | 5s stop after_a");
}

TEST_CASE("events: beginEvent, endEvent")
{
    auto runner = EngineRunner::body(R"x(<par>
        <img xml:id="a" src="a.jpg" dur="2s"/>
        <img xml:id="b" src="b.jpg" begin="a.beginEvent+1s" dur="1s"/>
        <img xml:id="c" src="c.jpg" begin="a.endEvent" dur="1s"/>
    </par>)x");
    CHECK(runner.run(10s) == "0s start a | 1s start b | 2s stop a | 2s stop b | 2s start c | 3s stop c");
}

TEST_CASE("activateEvent and click: touch on a medium")
{
    auto runner = EngineRunner::body(R"x(<par>
        <img xml:id="button" src="b.jpg" dur="indefinite"/>
        <img xml:id="info" src="i.jpg" begin="button.activateEvent" dur="2s"/>
        <img xml:id="later" src="l.jpg" begin="button.click+1s" dur="1s"/>
    </par>)x");
    CHECK(runner.run(2s) == "0s start button");
    runner.engine->elementActivated(*lastInstance(runner, "button"));
    CHECK(runner.runFor(6s) == "0s start button | 2s start info | 3s start later | 4s stop info | 4s stop later");
}

TEST_CASE("accesskey: every listener reacts, begin and end on the same key")
{
    auto runner = EngineRunner::body(R"x(<par>
        <img xml:id="standby" src="s.jpg" dur="indefinite" end="accesskey(a)"/>
        <img xml:id="one" src="1.jpg" begin="accesskey(a)" dur="2s"/>
        <audio xml:id="two" src="2.mp3" begin="accesskey(a)" dur="2s"/>
    </par>)x");
    runner.run(1s);
    runner.engine->accessKey('A'); // upper case counts too
    CHECK(runner.runFor(5s) == "0s start standby | 1s stop standby | 1s start one | 1s start two | 3s stop one | 3s stop two");
}

TEST_CASE("notify from outside")
{
    auto runner = EngineRunner::body(R"x(<par><img xml:id="alarm" src="a.jpg" begin="notify(fire)" dur="2s"/></par>)x");
    runner.run(1s);
    runner.engine->notify("smoke"); // nobody listens
    runner.engine->notify("fire");
    CHECK(runner.runFor(5s) == "1s start alarm | 3s stop alarm");
}

TEST_CASE("a trigger reaches only elements activated in a playing parent")
{
    auto runner = EngineRunner::body(R"x(
        <img xml:id="first" src="f.jpg" dur="3s"/>
        <par><img xml:id="late" src="l.jpg" begin="accesskey(x)" dur="1s"/></par>)x");
    runner.run(1s);
    runner.engine->accessKey('x'); // the par has not started yet
    CHECK(runner.runFor(5s) == "0s start first | 3s stop first");
}

TEST_CASE("repeatCount fires no new begin (standard)")
{
    auto runner = EngineRunner::body(R"x(<par>
        <img xml:id="a" src="a.jpg" dur="1s" repeatCount="3"/>
        <img xml:id="b" src="b.jpg" begin="a.begin" dur="indefinite" restart="always"/>
    </par>)x");
    CHECK(runner.run(5s) == "0s start a | 0s start b | 1s repeat a | 2s repeat a | 3s stop a");
}

TEST_CASE("negative trigger offset: began that long ago, plays the rest (standard)")
{
    auto runner = EngineRunner::body(R"x(<par><img xml:id="a" src="a.jpg" begin="accesskey(k)-1s" dur="3s"/></par>)x");
    runner.run(1s);
    runner.engine->accessKey('k');
    CHECK(runner.runFor(5s) == "1s start a | 3s stop a");
}

TEST_CASE("a trigger in the end list leaves the end open: later begins stay valid (standard)")
{
    auto runner = EngineRunner::body(R"x(<par>
        <img xml:id="a" src="a.jpg" begin="0s;4s" end="2s;accesskey(e)" dur="indefinite"/>
    </par>)x");
    CHECK(runner.run(10s) == "0s start a | 2s stop a | 4s start a");
}

TEST_CASE("end list with several times: an end before a later begin does not void it (standard)")
{
    auto runner = EngineRunner::body(R"x(<par><img xml:id="a" src="a.jpg" begin="0s;10s" end="5s;20s" dur="indefinite"/></par>)x");
    CHECK(runner.run(30s) == "0s start a | 5s stop a | 10s start a | 20s stop a");
}

TEST_CASE("wallclock: start at the time, a past one plays the rest of its dur")
{
    TimeZone tz("UTC"); // the FakeClock starts at 2026-01-01T00:00:00
    auto     later = EngineRunner::body(R"x(<par><img xml:id="a" src="a.jpg" begin="wallclock(2026-01-01T00:00:05)" dur="2s"/></par>)x");
    CHECK(later.run(10s) == "5s start a | 7s stop a");

    auto past = EngineRunner::body(R"x(<par><img xml:id="a" src="a.jpg" begin="wallclock(2025-12-31T23:59:58)" dur="5s"/></par>)x");
    CHECK(past.run(10s) == "0s start a | 3s stop a");

    auto zone = EngineRunner::body(R"x(<par><img xml:id="a" src="a.jpg" begin="wallclock(2026-01-01T01:00:03+01:00)" dur="1s"/></par>)x");
    CHECK(zone.run(10s) == "3s start a | 4s stop a");
}

TEST_CASE("wallclock repeating: every period, repeatCount ignored, end at a wallclock too")
{
    TimeZone tz("UTC");
    auto     runner = EngineRunner::body(R"x(<par>
        <img xml:id="a" src="a.jpg" begin="wallclock(R/2026-01-01T00:00:00/PT10S)" dur="2s" repeatCount="3"/>
        <img xml:id="b" src="b.jpg" begin="0s" end="wallclock(2026-01-01T00:00:15)" dur="indefinite"/>
    </par>)x");
    CHECK(runner.run(25s) == "0s start a | 0s start b | 2s stop a | 10s start a | 12s stop a | 15s stop b"
                             " | 20s start a | 22s stop a");
}

TEST_CASE("par/trigger/1_syncbase.smil: image with the video, end image after it")
{
    auto runner = EngineRunner::file("par/trigger/1_syncbase.smil");
    runner.media_length["../../../videos/0001.mp4"] = 4s;
    auto timeline = runner.run(12s);
    CHECK(timeline.find("1s start video0001") != std::string::npos);
    CHECK(timeline.find("1s start img_video") != std::string::npos);
    CHECK(timeline.find("5s stop img_video") != std::string::npos);
    CHECK(timeline.find("5s start img_end") != std::string::npos);
    CHECK(timeline.find("7s stop img_end") != std::string::npos);
}

TEST_CASE("par/trigger/2_events.smil: begin, 1, 2, 3, 4, end (stops begin)")
{
    auto runner = EngineRunner::file("par/trigger/2_events.smil");
    CHECK(runner.run(8s) == "0s start begin | 0s start img_25_6 | 1s stop img_25_6 | 1s start img_26_7 | 2s stop img_26_7"
                            " | 2s start img_27_8 | 3s stop img_27_8 | 3s start img_28_9 | 4s stop img_28_9"
                            " | 4s start end | 4s stop begin");
}

TEST_CASE("par/5_wallclock.smil: every 15 s, the three regions at :00, :05, :10")
{
    TimeZone tz("UTC");
    auto     runner   = EngineRunner::file("par/5_wallclock.smil");
    auto     timeline = runner.run(31s);
    for (const char *entry : {"0s start Aimg0001", "15s start Aimg0001", "30s start Aimg0001", "0s start imgAbegin",
                              "5s start imgBbegin", "10s start imgCbegin", "15s start imgAbegin", "20s start imgBbegin",
                              "25s start imgCbegin", "30s start imgAbegin"})
        CHECK(timeline.find(entry) != std::string::npos);
}

TEST_CASE("excl/trigger/3_activate_event.smil: touch on standby starts the count 2 s later")
{
    auto runner = EngineRunner::file("excl/trigger/3_activate_event.smil");
    runner.run(1s);
    runner.engine->elementActivated(*lastInstance(runner, "standby"));
    auto timeline = runner.runFor(7s);
    CHECK(timeline.find("3s stop standby | 3s start img_35_9") != std::string::npos);
    CHECK(timeline.find("6s start img_35_9") != std::string::npos); // repeated
}

TEST_CASE("excl/trigger/4_accesskey.smil: w stops standby, pressing again restarts the count")
{
    auto runner = EngineRunner::file("excl/trigger/4_accesskey.smil");
    runner.run(3s);
    runner.engine->accessKey('w');
    runner.runFor(5500ms);
    runner.engine->accessKey('w');
    auto timeline = runner.runFor(7s);
    CHECK(timeline.find("3s stop standby3 | 3s start img_36_9") != std::string::npos);
    CHECK(timeline.find("5.500s stop img_38_11 | 5.500s start img_36_9") != std::string::npos);
}

TEST_CASE("excl/trigger/7_notify.smil")
{
    auto runner = EngineRunner::file("excl/trigger/7_notify.smil");
    runner.run(3s);
    runner.engine->notify("heidewitzka");
    CHECK(runner.runFor(5s).find("3s stop standby3 | 3s start img_36_9") != std::string::npos);
}
