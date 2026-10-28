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

#include <ranges>

using namespace garlic::core::smil;
using namespace garlic::core::tests;
using namespace std::chrono_literals;

namespace
{
    /**
     * What becomes visible, in order: every start and resume, without the ones paused in the same moment (displaced at
     * once by a peer with "pause"). That is what the comments of the excl test files describe.
     */
    std::string visible(const std::string &timeline)
    {
        std::vector<std::string_view> entries;
        for (auto part : std::views::split(timeline, std::string_view(" | ")))
            entries.emplace_back(part.begin(), part.end());

        std::string result;
        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            std::string_view entry = entries[i];
            auto             time  = entry.substr(0, entry.find(' '));
            auto             verb  = entry.find(' ', time.size() + 1);
            auto             name  = entry.substr(verb + 1); // ids may contain spaces ("img begin")
            bool shown = entry.find(" start ") != std::string_view::npos || entry.find(" resume ") != std::string_view::npos;
            if (!shown)
                continue;
            bool paused_at_once = false;
            for (std::size_t j = i + 1; j < entries.size() && entries[j].starts_with(time); ++j)
                paused_at_once = paused_at_once || entries[j] == std::string(time) + " pause " + std::string(name);
            if (!paused_at_once)
                result += (result.empty() ? "" : ",") + std::string(name);
        }
        return result;
    }

    std::string run(const std::string &file)
    {
        auto runner = EngineRunner::file(file);
        return visible(runner.run(60s));
    }
}

TEST_CASE("excl without priorityClass: peers stop by default")
{
    auto runner = EngineRunner::body(R"(<excl>
        <img xml:id="a" src="a.jpg" begin="0s" dur="5s"/>
        <img xml:id="b" src="b.jpg" begin="1s" dur="1s"/>
    </excl><img xml:id="next" src="n.jpg" dur="1s"/>)");
    CHECK(runner.run(10s) == "0s start a | 1s stop a | 1s start b | 2s stop b | 2s start next | 3s stop next");
}

TEST_CASE("peers pause: the paused one goes on with its rest")
{
    auto runner = EngineRunner::body(R"(<excl><priorityClass peers="pause">
        <img xml:id="a" src="a.jpg" begin="0s" dur="4s"/>
        <img xml:id="b" src="b.jpg" begin="1s" dur="2s"/>
    </priorityClass></excl>)");
    CHECK(runner.run(10s) == "0s start a | 1s pause a | 1s start b | 3s stop b | 3s resume a | 6s stop a");
}

TEST_CASE("peers defer: the new one starts anew when it is its turn")
{
    auto runner = EngineRunner::body(R"(<excl><priorityClass peers="defer">
        <img xml:id="a" src="a.jpg" begin="0s" dur="3s"/>
        <img xml:id="b" src="b.jpg" begin="1s" dur="2s"/>
    </priorityClass></excl>)");
    CHECK(runner.run(10s) == "0s start a | 3s stop a | 3s start b | 5s stop b");
}

TEST_CASE("peers never: the new one is ignored like a begin ignored by restart")
{
    auto runner = EngineRunner::body(R"(<excl><priorityClass peers="never">
        <img xml:id="a" src="a.jpg" begin="0s" dur="3s"/>
        <img xml:id="b" src="b.jpg" begin="1s" dur="1s"/>
    </priorityClass></excl><img xml:id="next" src="n.jpg" dur="1s"/>)");
    CHECK(runner.run(10s) == "0s start a | 3s stop a | 3s start next | 4s stop next");
}

TEST_CASE("higher and lower: the class of the playing element decides")
{
    auto higher = EngineRunner::body(R"(<excl>
        <priorityClass><img xml:id="high" src="h.jpg" begin="1s" dur="1s"/></priorityClass>
        <priorityClass higher="stop"><img xml:id="low" src="l.jpg" begin="0s" dur="5s"/></priorityClass>
    </excl>)");
    CHECK(higher.run(10s) == "0s start low | 1s stop low | 1s start high | 2s stop high");

    auto lower = EngineRunner::body(R"(<excl>
        <priorityClass lower="defer"><img xml:id="high" src="h.jpg" begin="0s" dur="3s"/></priorityClass>
        <priorityClass><img xml:id="low" src="l.jpg" begin="1s" dur="1s"/></priorityClass>
    </excl>)");
    CHECK(lower.run(10s) == "0s start high | 3s stop high | 3s start low | 4s stop low");
}

TEST_CASE("queue: highest class first, within a class the last queued")
{
    auto runner = EngineRunner::body(R"(<excl>
        <priorityClass><img xml:id="top" src="t.jpg" begin="1s" dur="1s"/></priorityClass>
        <priorityClass peers="pause" higher="pause">
            <img xml:id="first" src="1.jpg" begin="0s" dur="2s"/>
            <img xml:id="second" src="2.jpg" begin="0.5s" dur="2s"/>
        </priorityClass>
    </excl>)");
    // second pauses first; top pauses second; then second (last queued), then first
    CHECK(visible(runner.run(20s)) == "first,second,top,second,first");
}

TEST_CASE("pausing a seq freezes everything in it")
{
    auto runner = EngineRunner::body(R"(<excl><priorityClass peers="pause">
        <seq begin="0s"><img xml:id="a" src="a.jpg" dur="2s"/><img xml:id="b" src="b.jpg" dur="2s"/></seq>
        <img xml:id="x" src="x.jpg" begin="1s" dur="3s"/>
    </priorityClass></excl>)");
    CHECK(runner.run(20s) == "0s start a | 1s pause a | 1s start x | 4s stop x | 4s resume a | 5s stop a"
                             " | 5s start b | 7s stop b");
}

TEST_CASE("pause: end times go on counting and fire at the resume if due")
{
    auto runner = EngineRunner::body(R"(<excl><priorityClass peers="pause">
        <img xml:id="a" src="a.jpg" begin="0s" end="2s"/>
        <img xml:id="b" src="b.jpg" begin="1s" dur="3s"/>
    </priorityClass></excl>)");
    // a would end at 2 s while paused: at the resume at 4 s it ends at once and is never shown again
    CHECK(runner.run(10s) == "0s start a | 1s pause a | 1s start b | 4s stop b | 4s stop a");
}

TEST_CASE("a skipped element displaces nobody")
{
    auto runner = EngineRunner::body(R"(<excl>
        <img xml:id="a" src="a.jpg" begin="0s" dur="3s"/>
        <img xml:id="missing" src="m.jpg" begin="1s" dur="1s"/>
    </excl>)");
    runner.media.missing.insert("m.jpg");
    CHECK(runner.run(10s) == "0s start a | 3s stop a");
}

TEST_CASE("excl endsync first: a stopped element is no end, the stopping one is")
{
    auto runner = EngineRunner::body(R"(<excl endsync="first">
        <img xml:id="a" src="a.jpg" begin="0s" dur="5s"/>
        <img xml:id="b" src="b.jpg" begin="1s" dur="1s"/>
        <img xml:id="c" src="c.jpg" begin="3s" dur="1s"/>
    </excl><img xml:id="next" src="n.jpg" dur="1s"/>)");
    CHECK(runner.run(10s) == "0s start a | 1s stop a | 1s start b | 2s stop b | 2s start next | 3s stop next");
}

TEST_CASE("empty excl is skipped like an empty seq, an excl with dur waits for it")
{
    auto runner = EngineRunner::body(R"(<excl/><excl dur="2s"/><img xml:id="a" src="a.jpg" dur="1s"/>)");
    CHECK(runner.run(10s) == "2.100s start a | 3.100s stop a"); // 100 ms for the skipped one, 2 s pause
}

TEST_CASE("excl test files as described in their comments")
{
    CHECK(run("excl/peers/1_stop.smil") == "img begin,imgA0001,imgA0002,imgA0005,imgA0001,imgA0006,imgA0007,imgA0008,"
                                          "imgA0009,imgA0010,imgA0006,imgB0005,imgB0006,imgB0007,imgB0008,imgB0009,"
                                          "imgB0010,imgC0006,imgC0007,imgC0008,imgC0009,imgC0010,img_end");
    CHECK(run("excl/peers/2_pause.smil") == "img begin,imgA0005,imgA0004,imgA0003,imgA0002,imgA0001,imgA0005,imgA0006,"
                                           "imgA0007,imgA0008,imgA0009,imgA0010,imgB0005,imgB0006,imgB0007,imgB0008,"
                                           "imgB0009,imgB0010,imgC0006,imgC0007,imgC0008,imgC0009,imgC0010,imgC0001,"
                                           "imgC0002,imgC0003,imgC0004,imgC0005,img_end");
    CHECK(run("excl/peers/3_defer.smil") == "img begin,imgA0001,imgA0005,imgA0004,imgA0003,imgA0002,imgA0005,imgA0006,"
                                           "imgA0007,imgA0008,imgA0009,imgA0010,imgB0001,imgB0006,imgB0007,imgB0008,"
                                           "imgB0009,imgB0010,imgC0001,imgC0002,imgC0003,imgC0004,imgC0005,imgC0006,"
                                           "imgC0007,imgC0008,imgC0009,imgC0010,img_end");
    CHECK(run("excl/peers/4_never.smil") == "img begin,imgA0001,imgA0005,imgA0006,imgA0007,imgA0008,imgA0009,imgA0010,"
                                           "imgB0001,imgB0006,imgB0007,imgB0008,imgB0009,imgB0010,imgC0001,imgC0002,"
                                           "imgC0003,imgC0004,imgC0005,img_end");
    CHECK(run("excl/higher/1_stop.smil") == "img begin,imgA0010,imgA0009,imgA0008,imgA0005,imgA0004,imgA0003,imgA0002,"
                                            "imgA0001,img_end");
    CHECK(run("excl/higher/2_pause.smil") == "img begin,imgA0010,imgA0009,imgA0008,imgA0005,imgA0004,imgA0003,imgA0002,"
                                             "imgA0001,imgA0002,imgA0003,imgA0004,imgA0005,imgA0007,imgA0006,img_end");
    CHECK(run("excl/lower/1_defer.smil") == "img begin,imgB0001,imgB0002,imgB0003,imgB0004,imgB0005,imgA0001,imgA0002,"
                                           "imgA0003,imgA0004,imgA0005,img_end");
    CHECK(run("excl/lower/2_never.smil") == "img begin,imgA0002,imgA0005,imgB0002,imgB010,imgC0001,imgC0002,imgC0006,"
                                           "imgC0007,imgC0008,imgC0009,imgC0010,img_end");
}
