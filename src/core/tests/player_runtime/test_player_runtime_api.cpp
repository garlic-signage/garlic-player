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

#include "common/memory_file_system.hpp"
#include "fake_clock.hpp"
#include "fake_http_transport.hpp"
#include "player_runtime/player_runtime_api.hpp"
#include "time_zone.hpp"

#include <map>

using namespace garlic::core;
using garlic::core::tests::FakeClock;
using garlic::core::tests::TimeZone;
using namespace std::chrono_literals;

namespace
{
    /// An index like the hub sends it: the body repeats, so media that are still downloading follow when they are there.
    std::string indexText(std::string_view body, int refresh = 60, std::string_view head = "")
    {
        return "<smil><head><meta http-equiv=\"Refresh\" content=\"" + std::to_string(refresh) + "\"/>" +
               std::string(head) + "</head><body repeatCount=\"indefinite\">" + std::string(body) + "</body></smil>";
    }

    struct Fixture
    {
        FakeClock                            clock;
        common::MemoryFileSystem             fs{clock};
        downloader::tests::FakeHttpTransport transport;
        std::unique_ptr<PlayerRuntimeApi>    runtime;
        std::vector<std::string>             log; // what the frontend saw
        std::map<std::string, smil::InstanceId> instances; // last instance per id
        NewLayout                            layout;    // kept until the next one, like a frontend

        explicit Fixture(std::string ini = "[httpd]\nenabled=false\n")
        {
            write("config/garlic-player.ini", ini);
            PlayerRuntimeConfig config;
            config.ini_path    = "config/garlic-player.ini";
            config.cache_dir   = "cache";
            config.log_dir     = "logs";
            config.version     = "2.0";
            config.index_uri   = "http://hub/index.smil";
            config.database_path = ":memory:";
            config.system_info = [] { return reports::SystemInfo{}; };
            config.free_space  = [] { return std::uint64_t{1} << 40; };
            runtime            = std::make_unique<PlayerRuntimeApi>(fs, clock, transport, config);
        }

        void write(const std::string &path, std::string_view text)
        {
            auto file = fs.openForWrite(path, common::WriteMode::Truncate);
            REQUIRE(file.has_value());
            REQUIRE((*file)->write(std::as_bytes(std::span(text.data(), text.size()))).has_value());
            REQUIRE((*file)->close().has_value());
        }

        void serve(const std::string &url, std::string body, const std::string &type, const std::string &etag = "v1")
        {
            downloader::tests::FakeHttpTransport::Response r;
            r.headers.add("Content-Type", type);
            r.headers.add("ETag", "\"" + etag + "\"");
            r.body       = std::move(body);
            r.chunk_size = 1 << 20;
            transport.respond(downloader::HttpMethod::Head, url, r);
            transport.respond(downloader::HttpMethod::Get, url, r);
        }

        /// The frontend loop: poll, record, sleep until the next deadline (at most until "until").
        void run(std::chrono::milliseconds duration)
        {
            auto end = clock.monotonic() + duration;
            while (true)
            {
                for (const CoreEvent &event : runtime->poll())
                    record(event);
                auto next = runtime->nextDeadline();
                if (!next || *next > end)
                    break;
                clock.advance(std::max(*next - clock.monotonic(), std::chrono::steady_clock::duration(1ms)));
            }
            if (clock.monotonic() < end)
                clock.advance(end - clock.monotonic());
        }

        void record(const CoreEvent &event)
        {
            if (const auto *start = std::get_if<smil::StartMedia>(&event))
            {
                log.push_back("start " + start->media->id);
                instances[start->media->id] = start->id;
            }
            else if (std::holds_alternative<smil::StopMedia>(event))
                log.push_back("stop");
            else if (const auto *next = std::get_if<NewLayout>(&event))
            {
                layout = *next;
                log.push_back("layout " + layout.document->head.title);
            }
            else if (const auto *background = std::get_if<BackgroundImage>(&event))
                log.push_back("background " + background->src);
            else if (const auto *reboot = std::get_if<tasks::Reboot>(&event))
                log.push_back("reboot " + reboot->task_id);
            else if (const auto *clear = std::get_if<tasks::ClearCaches>(&event))
                log.push_back(std::string("clear ") + (clear->player ? "player" : "") + (clear->web ? " web" : ""));
            else if (std::holds_alternative<tasks::InstallSoftware>(event))
                log.push_back("install");
        }

        std::string seen() const
        {
            std::string text;
            for (const auto &entry : log)
                text += (text.empty() ? "" : " | ") + entry;
            return text;
        }
    };
}

TEST_CASE("start: index downloaded, NewLayout, media downloaded and started")
{
    Fixture f;
    f.serve("http://hub/index.smil",
            indexText(R"x(<img xml:id="a" src="img/a.jpg" dur="10s"/>)x", 60, R"x(<meta name="title" content="first"/>)x"),
            "application/smil+xml");
    f.serve("http://hub/img/a.jpg", "jpeg", "image/jpeg");
    REQUIRE(f.runtime->start().has_value());
    f.run(5s);

    CHECK(f.seen() == "layout first | start a");
    CHECK(f.runtime->configuration().lastPlayedIndexPath() == "cache/index.smil");
}

TEST_CASE("a new index: stop the old one, NewLayout, start the new one - in this order")
{
    Fixture f;
    f.serve("http://hub/index.smil", indexText(R"x(<img xml:id="a" src="a.jpg" dur="indefinite"/>)x", 60,
                                          R"x(<meta name="title" content="v1"/>)x"),
            "application/smil+xml", "v1");
    f.serve("http://hub/a.jpg", "a", "image/jpeg");
    f.serve("http://hub/b.jpg", "b", "image/jpeg");
    REQUIRE(f.runtime->start().has_value());
    f.run(5s);
    REQUIRE(f.seen() == "layout v1 | start a");

    f.serve("http://hub/index.smil", indexText(R"x(<img xml:id="b" src="b.jpg" dur="indefinite"/>)x", 60,
                                          R"x(<meta name="title" content="v2"/>)x"),
            "application/smil+xml", "v2");
    f.run(70s);
    CHECK(f.seen() == "layout v1 | start a | stop | layout v2 | start b");
}

TEST_CASE("a broken new index: the old one plays on")
{
    Fixture f;
    f.serve("http://hub/index.smil", indexText(R"x(<img xml:id="a" src="a.jpg" dur="indefinite"/>)x"), "application/smil+xml",
            "v1");
    f.serve("http://hub/a.jpg", "a", "image/jpeg");
    REQUIRE(f.runtime->start().has_value());
    f.run(5s);

    f.serve("http://hub/index.smil", "<smil><body>", "application/smil+xml", "v2");
    f.run(70s);
    CHECK(f.seen() == "layout No Title | start a");
}

TEST_CASE("input of the frontend goes to the engine: end of a video, a key")
{
    Fixture f;
    f.serve("http://hub/index.smil", indexText(R"x(<par>
                                               <seq repeatCount="indefinite"><video xml:id="v" src="v.mp4"/></seq>
                                               <img xml:id="k" src="k.jpg" begin="accesskey(k)" dur="1s"/></par>)x"),
            "application/smil+xml");
    f.serve("http://hub/v.mp4", "video", "video/mp4");
    f.serve("http://hub/k.jpg", "key", "image/jpeg");
    REQUIRE(f.runtime->start().has_value());
    f.run(5s);
    REQUIRE(f.seen() == "layout No Title | start v");

    f.runtime->mediaFinished(f.instances["v"]); // the video has ended: the seq plays it again
    f.run(100ms);
    CHECK(f.seen() == "layout No Title | start v | stop | start v");

    f.runtime->accessKey('K');
    f.run(100ms);
    CHECK(f.seen() == "layout No Title | start v | stop | start v | start k");
}

TEST_CASE("a task from the hub: restart goes to the host, clearing the player cache is done by the core")
{
    Fixture f;
    f.serve("http://hub/index.smil", indexText(R"x(<img xml:id="a" src="a.jpg" dur="indefinite"/>)x", 60,
                                          R"x(<metadata><subscription><type>TaskSchedule</type><action>http://hub/tasks.xml</action></subscription></metadata>)x"),
            "application/smil+xml");
    f.serve("http://hub/a.jpg", "a", "image/jpeg");
    f.serve("http://hub/tasks.xml", R"x(<s><timedTaskList><applyCommand id="c1"><command>clear_cache</command></applyCommand>
                                        </timedTaskList></s>)x",
            "application/xml");
    REQUIRE(f.runtime->start().has_value());
    f.run(5s);

    CHECK(f.seen().find("clear player") != std::string::npos);
    CHECK_FALSE(f.fs.exists("cache/index.smil"));
}

TEST_CASE("scheduled restart from the configuration")
{
    TimeZone tz("UTC"); // the FakeClock starts on Thursday, 2026-01-01 00:00
    Fixture  f("[httpd]\nenabled=false\n[schedule]\nreboot_days=thursday\nreboot_time=00:00:30\n");
    f.serve("http://hub/index.smil", indexText(""), "application/smil+xml");
    REQUIRE(f.runtime->start().has_value());
    f.run(20s);
    CHECK(f.seen().find("reboot") == std::string::npos);
    f.run(20s);
    CHECK(f.seen().find("reboot DailyReboot") != std::string::npos);
}

TEST_CASE("nextDeadline: fast while downloading, idle without anything to do")
{
    Fixture f;
    f.serve("http://hub/index.smil", indexText(R"x(<img xml:id="a" src="a.jpg" dur="indefinite"/>)x", 0), "application/smil+xml");
    f.serve("http://hub/a.jpg", "a", "image/jpeg");
    REQUIRE(f.runtime->start().has_value());
    auto busy = f.runtime->nextDeadline();
    REQUIRE(busy.has_value());
    CHECK(*busy <= f.clock.monotonic() + PlayerRuntimeApi::busy_poll);

    f.run(5s);
    auto idle = f.runtime->nextDeadline();
    REQUIRE(idle.has_value()); // only the resource log
    CHECK(*idle - f.clock.monotonic() > 200s);
}

TEST_CASE("background images: in NewLayout when they are there, otherwise as BackgroundImage when downloaded")
{
    Fixture f;
    f.serve("http://hub/index.smil", indexText("", 60, R"x(<layout><root-layout backgroundImage="/local/root.png"/>
            <region xml:id="r" backgroundImage="bg.png"/><region xml:id="s" backgroundImage="none"/></layout>)x"),
            "application/smil+xml");
    f.serve("http://hub/bg.png", "png", "image/png");
    REQUIRE(f.runtime->start().has_value());
    f.run(5s);

    CHECK(f.seen() == "layout No Title | background bg.png");
    CHECK(f.layout.backgrounds.size() == 1); // the local one at once, "none" is no image
    CHECK(f.layout.backgrounds.at("/local/root.png") == "/local/root.png");

    // a new index with the same image: now it comes with NewLayout
    f.log.clear();
    f.serve("http://hub/index.smil", indexText("", 60, R"x(<layout><region xml:id="r" backgroundImage="bg.png"/></layout>)x"),
            "application/smil+xml", "v2");
    f.run(60s);
    CHECK(f.seen() == "layout No Title");
    REQUIRE(f.layout.backgrounds.contains("bg.png"));
    CHECK(f.fs.content(f.layout.backgrounds.at("bg.png")) == "png");
}

TEST_CASE("the HTTP server runs when it is enabled")
{
    Fixture f("[httpd]\nenabled=true\nhost=127.0.0.1\nport=0\n");
    REQUIRE(f.runtime->start().has_value());
    f.run(1s);
    auto next = f.runtime->nextDeadline();
    REQUIRE(next.has_value());
    CHECK(*next <= f.clock.monotonic() + PlayerRuntimeApi::busy_poll); // polled for the server
}
