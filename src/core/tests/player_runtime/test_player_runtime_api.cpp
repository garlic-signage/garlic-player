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
#include "httpd/mhd_server.hpp"
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

TEST_CASE("player settings: read, an invalid index URI is not stored")
{
    Fixture f;
    REQUIRE(f.runtime->start().has_value());
    PlayerSettings current = f.runtime->playerSettings();
    CHECK(current.index_uri == "http://hub/index.smil");
    CHECK_FALSE(current.httpd_enabled);  // the ini of the fixture
    CHECK(current.httpd_port == 8080);   // default

    PlayerSettings next = current;
    next.player_name    = "lobby";
    next.index_uri      = "nothing";
    auto changed        = f.runtime->changePlayerSettings(next);
    REQUIRE_FALSE(changed.has_value());
    CHECK_FALSE(changed.error().empty());
    CHECK(f.runtime->playerSettings().player_name == current.player_name);

    next.index_uri     = "http://hub/index.smil";
    next.httpd_port    = 0;
    next.httpd_enabled = true;
    CHECK(f.runtime->changePlayerSettings(next).error() == "invalid port");
}

TEST_CASE("player settings: stored, the user agent follows the name, the index is loaded again")
{
    Fixture f;
    f.serve("http://hub/index.smil", indexText(R"x(<img xml:id="a" src="http://hub/a.jpg" dur="10s"/>)x"), "application/smil+xml");
    f.serve("http://hub/other.smil", indexText(R"x(<img xml:id="b" src="http://hub/a.jpg" dur="10s"/>)x"), "application/smil+xml");
    f.serve("http://hub/a.jpg", "jpg", "image/jpeg");
    REQUIRE(f.runtime->start().has_value());
    f.run(5s);

    PlayerSettings next = f.runtime->playerSettings();
    next.player_name    = "lobby";
    next.index_uri      = "http://hub/other.smil";
    REQUIRE(f.runtime->changePlayerSettings(next).has_value());
    f.run(5s);

    CHECK(f.runtime->configuration().playerName() == "lobby");
    CHECK(f.runtime->configuration().indexUri() == "http://hub/other.smil");
    CHECK(f.fs.content("config/garlic-player.ini")->find("player_name=lobby") != std::string::npos);
    CHECK(f.instances.contains("b"));

    auto other = std::ranges::find(f.transport.requests, std::string("http://hub/other.smil"), &downloader::HttpRequest::url);
    REQUIRE(other != f.transport.requests.end());
    CHECK(other->headers.find("User-Agent").value_or("").find("NAME:lobby") != std::string::npos);
}

TEST_CASE("player settings: the HTTP server is switched on and off, a port in use keeps the old one")
{
    Fixture f;
    REQUIRE(f.runtime->start().has_value());

    // a port that is surely in use: the one of a second server
    httpd::Router        router;
    httpd::MhdServer     blocker(router, f.fs);
    httpd::HttpdSettings blocking; // all interfaces like the player
    blocking.port = 0;
    REQUIRE(blocker.start(blocking).has_value());

    PlayerSettings next = f.runtime->playerSettings();
    next.httpd_enabled  = true;
    next.httpd_port     = blocker.port();
    auto changed        = f.runtime->changePlayerSettings(next);
    REQUIRE_FALSE(changed.has_value());
    CHECK(changed.error().find(std::to_string(blocker.port())) != std::string::npos);
    CHECK_FALSE(f.runtime->playerSettings().httpd_enabled); // nothing stored

    blocker.stop();
    REQUIRE(f.runtime->changePlayerSettings(next).has_value()); // now free
    CHECK(f.runtime->playerSettings().httpd_enabled);
    CHECK(f.runtime->playerSettings().httpd_port == next.httpd_port);
    CHECK(f.fs.content("config/garlic-player.ini")->find("enabled=true") != std::string::npos);

    next.httpd_enabled = false;
    REQUIRE(f.runtime->changePlayerSettings(next).has_value());
    CHECK_FALSE(f.runtime->playerSettings().httpd_enabled);
}

TEST_CASE("debug info: title, user agent, disk and memory of the system, the player with its maxima")
{
    Fixture f;
    f.serve("http://hub/index.smil",
            indexText("", 60, R"x(<meta name="title" content="lobby"/>)x"), "application/smil+xml");
    reports::SystemInfo system;
    system.total_capacity = 1000;
    system.free_space     = 400;
    system.memory_total   = 2048;
    system.memory_used    = 512;
    f.runtime.reset(); // again with a system info that knows something
    PlayerRuntimeConfig config;
    config.ini_path      = "config/garlic-player.ini";
    config.cache_dir     = "cache";
    config.log_dir       = "logs";
    config.index_uri     = "http://hub/index.smil";
    config.database_path = ":memory:";
    config.system_info   = [system] { return system; };
    config.free_space    = [] { return std::uint64_t{1} << 40; };
    f.runtime            = std::make_unique<PlayerRuntimeApi>(f.fs, f.clock, f.transport, config);
    REQUIRE(f.runtime->start().has_value());
    f.run(5s);

    DebugInfo info = f.runtime->debugInfo();
    CHECK(info.index_title == "lobby");
    CHECK(info.user_agent == f.runtime->configuration().userAgent());
    CHECK(info.disk_total == 1000u);
    CHECK(info.disk_free == 400u);
    CHECK(info.memory_total == 2048u);
    CHECK(info.memory_free == 1536u);
    CHECK(info.memory_max >= info.memory_used); // the test process itself on Linux
    CHECK(info.threads_max >= info.threads);
    if (info.memory_used > 0)
        CHECK_FALSE(info.memory_max_time.empty());
}

TEST_CASE("-c config.xml: applied before the first index, an index URI of the command line wins")
{
    Fixture f;
    f.write("usb/config.xml", R"(<configuration><prop name="info.playerName" value="Lobby"/>
        <prop name="content.serverUrl" value="http://hub/from-config.smil"/><prop name="volume" value="40"/></configuration>)");
    auto with_file = [&](std::string index_uri)
    {
        PlayerRuntimeConfig config;
        config.ini_path           = "config/garlic-player.ini";
        config.cache_dir          = "cache";
        config.log_dir            = "logs";
        config.index_uri          = std::move(index_uri);
        config.database_path      = ":memory:";
        config.configuration_file = "usb/config.xml";
        config.system_info        = [] { return reports::SystemInfo{}; };
        config.free_space         = [] { return std::uint64_t{1} << 40; };
        f.runtime                 = std::make_unique<PlayerRuntimeApi>(f.fs, f.clock, f.transport, config);
        REQUIRE(f.runtime->start().has_value());
        f.run(1s);
    };

    with_file("");
    CHECK(f.runtime->configuration().playerName() == "Lobby");
    CHECK(f.runtime->configuration().value("audio/volume") == "40");
    CHECK(f.runtime->configuration().indexUri() == "http://hub/from-config.smil");
    auto first = std::ranges::find(f.transport.requests, std::string("http://hub/from-config.smil"), &downloader::HttpRequest::url);
    REQUIRE(first != f.transport.requests.end());
    CHECK(first->headers.find("User-Agent").value_or("").find("NAME:Lobby") != std::string::npos);

    f.runtime.reset();
    with_file("http://hub/command-line.smil");
    CHECK(f.runtime->configuration().indexUri() == "http://hub/command-line.smil");
}

TEST_CASE("-c config.xml missing: an event, the player starts anyway")
{
    Fixture f;
    PlayerRuntimeConfig config;
    config.ini_path           = "config/garlic-player.ini";
    config.cache_dir          = "cache";
    config.log_dir            = "logs";
    config.index_uri          = "http://hub/index.smil";
    config.database_path      = ":memory:";
    config.configuration_file = "usb/missing.xml";
    config.system_info        = [] { return reports::SystemInfo{}; };
    f.runtime                 = std::make_unique<PlayerRuntimeApi>(f.fs, f.clock, f.transport, config);
    REQUIRE(f.runtime->start().has_value());
    CHECK(f.fs.content("logs/event_log.xml").value_or("").find("CONFIGURATION_OPEN_ERROR") != std::string::npos);
}
