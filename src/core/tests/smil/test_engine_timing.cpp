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
    std::size_t count(const std::string &text, const std::string &part)
    {
        std::size_t n = 0;
        for (auto pos = text.find(part); pos != std::string::npos; pos = text.find(part, pos + 1))
            ++n;
        return n;
    }
}

TEST_CASE("begin in a seq counts from the end of the previous child")
{
    auto runner = EngineRunner::body(R"(<img xml:id="a" src="a.jpg" dur="1s"/><img xml:id="b" src="b.jpg" begin="2s" dur="1s"/>)");
    CHECK(runner.run(10s) == "0s start a | 1s stop a | 3s start b | 4s stop b");
}

TEST_CASE("begin in a par counts from the start of the par")
{
    auto runner = EngineRunner::body(R"(<par><img xml:id="a" src="a.jpg" dur="3s"/><img xml:id="b" src="b.jpg" begin="1s" dur="1s"/></par>)");
    CHECK(runner.run(10s) == "0s start a | 1s start b | 2s stop b | 3s stop a");
}

TEST_CASE("begin list: a new interval (new instance) per begin, the par waits for the last one")
{
    auto runner = EngineRunner::body(R"(<par><img xml:id="a" src="a.jpg" begin="0s;3s" dur="1s"/></par>
                                       <img xml:id="next" src="n.jpg" dur="1s"/>)");
    CHECK(runner.run(10s) == "0s start a | 1s stop a | 3s start a | 4s stop a | 4s start next | 5s stop next");
    REQUIRE(runner.starts.size() == 3);
    CHECK(runner.starts[0].id != runner.starts[1].id);
}

TEST_CASE("negative begin: started in the past, plays the rest of its dur if there is one")
{
    auto rest = EngineRunner::body(R"(<par><img xml:id="a" src="a.jpg" begin="-2s" dur="5s"/></par>)");
    CHECK(rest.run(10s) == "0s start a | 3s stop a");

    auto over = EngineRunner::body(R"(<par><img xml:id="a" src="a.jpg" begin="-5s" dur="2s"/></par>
                                     <img xml:id="next" src="n.jpg" dur="1s"/>)");
    CHECK(over.run(10s) == "0.100s start next | 1.100s stop next"); // never starts: skipped

    auto indefinite = EngineRunner::body(R"(<par><video xml:id="v" src="v.mp4" begin="-2s" dur="indefinite"/></par>)");
    CHECK(indefinite.run(10s) == "0s start v");

    auto no_dur = EngineRunner::body(R"(<par><video xml:id="v" src="v.mp4" begin="-2s"/></par>
                                       <img xml:id="next" src="n.jpg" dur="1s"/>)");
    CHECK(no_dur.run(10s) == "0.100s start next | 1.100s stop next"); // like garlic-lib: no dur, no start
}

TEST_CASE("end cuts, but does not extend")
{
    auto cuts = EngineRunner::body(R"(<img xml:id="a" src="a.jpg" dur="10s" end="3s"/><img xml:id="b" src="b.jpg" dur="1s"/>)");
    CHECK(cuts.run(20s) == "0s start a | 3s stop a | 3s start b | 4s stop b");

    auto later = EngineRunner::body(R"(<img xml:id="a" src="a.jpg" dur="2s" end="5s"/><img xml:id="b" src="b.jpg" dur="1s"/>)");
    CHECK(later.run(20s) == "0s start a | 2s stop a | 2s start b | 3s stop b");

    auto indefinite = EngineRunner::body(R"(<img xml:id="a" src="a.jpg" dur="indefinite" end="4s"/><img xml:id="b" src="b.jpg" dur="1s"/>)");
    CHECK(indefinite.run(20s) == "0s start a | 4s stop a | 4s start b | 5s stop b");
}

TEST_CASE("end before begin and end 0s: never shown")
{
    auto before = EngineRunner::body(R"(<par><img xml:id="a" src="a.jpg" begin="5s" end="3s" dur="1s"/></par>
                                       <img xml:id="next" src="n.jpg" dur="1s"/>)");
    CHECK(before.run(10s) == "3s start next | 4s stop next");

    auto zero = EngineRunner::body(R"(<img xml:id="a" src="a.jpg" end="0s" dur="1s"/><img xml:id="next" src="n.jpg" dur="1s"/>)");
    CHECK(zero.run(10s) == "0s start next | 1s stop next");
}

TEST_CASE("restart: always starts a new interval, whenNotActive and never ignore it")
{
    auto always = EngineRunner::body(R"(<par><img xml:id="a" src="a.jpg" begin="0s;1s" dur="3s"/></par>)");
    CHECK(always.run(10s) == "0s start a | 1s stop a | 1s start a | 4s stop a");

    auto when_not_active = EngineRunner::body(R"(<par><img xml:id="a" src="a.jpg" begin="0s;1s;4s" dur="3s" restart="whenNotActive"/></par>)");
    CHECK(when_not_active.run(10s) == "0s start a | 3s stop a | 4s start a | 7s stop a");

    auto never = EngineRunner::body(R"(<par><img xml:id="a" src="a.jpg" begin="0s;2s;5s" dur="1s" restart="never"/></par>
                                      <img xml:id="next" src="n.jpg" dur="1s"/>)");
    CHECK(never.run(10s) == "0s start a | 1s stop a | 5s start next | 6s stop next"); // waits for the last begin
}

TEST_CASE("repeatCount is ignored while begin times are to come")
{
    auto runner = EngineRunner::body(R"(<par><img xml:id="a" src="a.jpg" begin="0s;5s" dur="1s" repeatCount="3"/></par>)");
    CHECK(runner.run(20s) == "0s start a | 1s stop a | 5s start a | 6s repeat a | 7s repeat a | 8s stop a");
}

TEST_CASE("expr: checked at every start, false skips the element")
{
    TimeZone tz("UTC");
    auto     runner = EngineRunner::body(R"x(
        <img xml:id="no" src="a.jpg" dur="1s" expr="smil-playerName() = 'Kitchen'"/>
        <img xml:id="yes" src="b.jpg" dur="1s" expr="smil-playerName() = 'Lobby'"/>)x");
    runner.config.player_name = "Lobby";
    CHECK(runner.run(10s) == "0.100s start yes | 1.100s stop yes");

    // false at 0 s, true from 2 s on (2026-01-01T00:00:00 is the start of the FakeClock)
    auto later = EngineRunner::body(R"x(<par><img xml:id="a" src="a.jpg" dur="1s" begin="0s;2s"
                                        expr="compare(adapi-date(), '2026-01-01T00:00:02') &gt;= 0"/></par>)x");
    CHECK(later.run(10s) == "2s start a | 3s stop a");
}

TEST_CASE("seq/5_media_repeats.smil")
{
    auto runner = EngineRunner::file("seq/5_media_repeats.smil");
    runner.media_length["../../videos/0001.mp4"] = 2s;
    // begin, 2s wait, 1 for 5s, 2 (wallclock not allowed in a seq: begins at once), 3, 3 video repeats, 5 short repeats, end
    CHECK(runner.run(30s) == "0s start img_begin | 1s stop img_begin"
                             " | 3s start img0001 | 4s repeat img0001 | 5s repeat img0001 | 6s repeat img0001"
                             " | 7s repeat img0001 | 8s stop img0001 | 8s start img0002 | 9s stop img0002"
                             " | 9s start img0003 | 10s stop img0003"
                             " | 10s start video_30_4 | 12s repeat video_30_4 | 14s repeat video_30_4 | 16s stop video_30_4"
                             " | 16s start video_31_5 | 17s repeat video_31_5 | 18s repeat video_31_5"
                             " | 19s repeat video_31_5 | 20s repeat video_31_5 | 21s stop video_31_5"
                             " | 21s start img_end | 22s stop img_end");
}

TEST_CASE("seq/6_conditional_play.smil: only in January 2022")
{
    TimeZone tz("UTC");
    auto     today = EngineRunner::file("seq/6_conditional_play.smil");
    // begin, 1, 2, 4, end; seq2 and img0003 skipped
    CHECK(today.run(10s) == "0s start img_begin | 1s stop img_begin | 1s start img0001 | 2s stop img0001"
                            " | 2.100s start img0002 | 3.100s stop img0002 | 3.200s start img0004 | 4.200s stop img0004"
                            " | 4.200s start img_end | 5.200s stop img_end");

    auto january = EngineRunner::file("seq/6_conditional_play.smil");
    january.clock.setWallTime(std::chrono::sys_days{std::chrono::January / 15 / 2022});
    auto timeline = january.run(10s);
    CHECK(timeline.find("start img0009") != std::string::npos);
    CHECK(timeline.find("start img0010") != std::string::npos);
    CHECK(timeline.find("start img0003") != std::string::npos);
}

TEST_CASE("par/6_endsync.smil: last, first and id")
{
    auto runner   = EngineRunner::file("par/6_endsync.smil");
    auto timeline = runner.run(12s);
    // last: begin, 1, 2, 3, 4, wait, end at 9 s, then again at 10 s
    CHECK(timeline.find("9s start imgBend") != std::string::npos);
    CHECK(timeline.find("10s start imgBbegin") != std::string::npos);
    // first: imgCbegin ends first at 1 s, so the par repeats every second and shows only begin
    CHECK(timeline.find("1s start imgCbegin") != std::string::npos);
    CHECK(timeline.find("start imgC0001") == std::string::npos);
    // id: seq3 ends at 5 s, so imgDend at 10 s is never shown
    CHECK(timeline.find("5s stop imgD0004") != std::string::npos);
    CHECK(timeline.find("start imgDend") == std::string::npos);
}

TEST_CASE("par/7_multi_begin_endsync.smil")
{
    auto runner   = EngineRunner::file("par/7_multi_begin_endsync.smil");
    auto timeline = runner.run(18s);
    // last: 0001 to 0004 four times, end at 16.4 s, then again
    CHECK(count(timeline, "start imgA0001") == 5); // 0, 4, 8, 12 and the next round at 17.4 s
    CHECK(timeline.find("16.400s start imgAend") != std::string::npos);
    CHECK(timeline.find("17.400s start imgAbegin") != std::string::npos);
    // first: imgBbegin ends after 1 s and ends the par: begin and 0001 every second
    CHECK(timeline.find("1s start imgBbegin") != std::string::npos);
    CHECK(timeline.find("start imgB0002") == std::string::npos);
}

TEST_CASE("par/8_multi_begin_restart.smil")
{
    auto runner   = EngineRunner::file("par/8_multi_begin_restart.smil");
    auto timeline = runner.run(16s);
    // never: begin, 1, 2, 3, end, then wait until the parent repeats at 16 s
    CHECK(count(timeline, "start imgAbegin") == 2);
    CHECK(timeline.find("16s start imgAbegin") != std::string::npos);
    // always: restarts at 2 s and at 11 s, also while playing
    CHECK(timeline.find("2s start imgBbegin") != std::string::npos);
    CHECK(timeline.find("11s start imgBbegin") != std::string::npos);
    CHECK(timeline.find("10s start imgBbegin") != std::string::npos);
    // whenNotActive: 0 s, 6 s and 11 s (the second ends just when the third begins)
    CHECK(timeline.find("6s start imgCbegin") != std::string::npos);
    CHECK(timeline.find("11s start imgCbegin") != std::string::npos);
    CHECK(count(timeline, "start imgCbegin") == 4); // and at 16 s with the parent
}
