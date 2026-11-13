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
#include "raw_http.hpp"
#include "time_zone.hpp"

#include <future>
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

    /// Records the calls in the log of the fixture, like the platform part of a player would pass them on.
    struct FakeSupervisor : Supervisor
    {
        std::vector<std::string> &log;

        explicit FakeSupervisor(std::vector<std::string> &l) : log(l) {}
        void reboot(const std::string &task_id) override { log.push_back("reboot " + task_id); }
        void installSoftware(const std::string &path) override { log.push_back("install " + path); }
    };

    struct Fixture
    {
        FakeClock                            clock;
        common::MemoryFileSystem             fs{clock};
        downloader::tests::FakeHttpTransport transport;
        std::unique_ptr<PlayerRuntimeApi>    runtime;
        std::vector<std::string>             log; // what the frontend saw
        std::map<std::string, smil::InstanceId> instances; // last instance per id
        NewLayout                            layout;    // kept until the next one, like a frontend
        FakeSupervisor                       supervisor{log};

        explicit Fixture(std::string ini = "[httpd]\nenabled=false\n")
        {
            write("config/garlic-player.ini", ini);
            runtime = std::make_unique<PlayerRuntimeApi>(fs, clock, transport, base());
        }

        /// The configuration of the fixture, to change before a runtime of its own.
        PlayerRuntimeConfig base()
        {
            PlayerRuntimeConfig config;
            config.ini_path      = "config/garlic-player.ini";
            config.cache_dir     = "cache";
            config.log_dir       = "logs";
            config.version       = "2.0";
            config.index_uri     = "http://hub/index.smil";
            config.database_path = ":memory:";
            config.system_info   = [] { return reports::SystemInfo{}; };
            config.free_space    = [] { return std::uint64_t{1} << 40; };
            config.supervisor    = &supervisor;
            return config;
        }

        std::string debugLog() const { return fs.content("logs/debug.log").value_or(""); }

        /// A request to the HTTP server of the runtime, polled here like the frontend does; the clock goes on.
        std::string http(std::uint16_t port, const std::string &request,
                         std::chrono::milliseconds step = std::chrono::milliseconds(0))
        {
            auto client = std::async(std::launch::async, [port, request] { return tests::sendRaw(port, request); });
            auto end    = std::chrono::steady_clock::now() + 5s;
            while (client.wait_for(1ms) != std::future_status::ready && std::chrono::steady_clock::now() < end)
            {
                for (const CoreEvent &event : runtime->poll())
                    record(event);
                clock.advance(step);
            }
            REQUIRE(client.wait_for(0s) == std::future_status::ready);
            return client.get();
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
            else if (const auto *shot = std::get_if<Screenshot>(&event))
                log.push_back("screenshot " + shot->path);
            else if (const auto *clear = std::get_if<tasks::ClearCaches>(&event))
                log.push_back(std::string("clear ") + (clear->player ? "player" : "") + (clear->web ? " web" : ""));
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

TEST_CASE("start: a web page is not downloaded, it plays from its URL")
{
    Fixture f;
    f.serve("http://hub/index.smil", indexText(R"x(<text xml:id="w" src="http://hub/page.html" dur="indefinite"/>)x"),
            "application/smil+xml");
    f.serve("http://hub/page.html", "<html>", "text/html");
    REQUIRE(f.runtime->start().has_value());
    f.run(5s);
    CHECK(f.seen() == "layout No Title | start w");
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

TEST_CASE("a task from the hub: the core clears the player cache, restarts when the frontend has cleared its cache")
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
    CHECK(f.seen().find("reboot") == std::string::npos); // waits for the frontend
    f.runtime->cachesCleared();
    CHECK(f.seen().ends_with("clear player | reboot c1"));
    f.runtime->cachesCleared(); // once only
    CHECK(f.seen().ends_with("clear player | reboot c1"));
    CHECK_FALSE(f.fs.exists("cache/index.smil"));
}

TEST_CASE("tasks from the hub: restart and installation go to the supervisor")
{
    Fixture f;
    f.serve("http://hub/index.smil", indexText("", 60,
                                          R"x(<metadata><subscription><type>TaskSchedule</type><action>http://hub/tasks.xml</action></subscription></metadata>)x"),
            "application/smil+xml");
    f.serve("http://hub/fw", "binary", "application/octet-stream");
    f.serve("http://hub/tasks.xml", R"x(<s><timedTaskList><firmwareUpdate id="f1"><sourceURI>http://hub/fw</sourceURI>
                                        <version>2.1</version></firmwareUpdate><shutdownPlayer id="r1"/>
                                        </timedTaskList></s>)x",
            "application/xml");
    REQUIRE(f.runtime->start().has_value());
    f.run(5s);

    CHECK(f.seen().find("reboot r1") != std::string::npos);
    CHECK(f.seen().find("install cache/garlic-player.") != std::string::npos);
}

TEST_CASE("without a supervisor a restart is only written to debug.log")
{
    TimeZone tz("UTC");
    Fixture  f("[httpd]\nenabled=false\n[schedule]\nreboot_days=thursday\nreboot_time=00:00:30\n");
    PlayerRuntimeConfig config;
    config.ini_path      = "config/garlic-player.ini";
    config.cache_dir     = "cache";
    config.log_dir       = "logs";
    config.version       = "2.0";
    config.index_uri     = "http://hub/index.smil";
    config.database_path = ":memory:";
    config.system_info   = [] { return reports::SystemInfo{}; };
    config.free_space    = [] { return std::uint64_t{1} << 40; };
    f.runtime            = std::make_unique<PlayerRuntimeApi>(f.fs, f.clock, f.transport, config);
    f.serve("http://hub/index.smil", indexText(""), "application/smil+xml");
    REQUIRE(f.runtime->start().has_value());
    f.run(40s);

    CHECK(f.seen().find("reboot") == std::string::npos);
    CHECK(f.fs.readFile("logs/debug.log").value_or("").find("none, no restart: DailyReboot") != std::string::npos);
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

// ------------------------------------------------------------------ start, poll and nextDeadline

TEST_CASE("start: the version is in the user agent of the downloads, without -c no configuration file is read")
{
    Fixture f;
    f.serve("http://hub/index.smil", indexText(""), "application/smil+xml");
    REQUIRE(f.runtime->start().has_value());
    f.run(1s);
    REQUIRE_FALSE(f.transport.requests.empty());
    CHECK(f.transport.requests[0].headers.find("User-Agent").value_or("").contains("/2.0 (MODEL:Garlic)"));
    CHECK_FALSE(f.fs.content("logs/event_log.xml").value_or("").contains("CONFIGURATION_OPEN_ERROR"));
}

TEST_CASE("before start(): poll, nextDeadline, input, settings and debug info do nothing")
{
    Fixture f;
    auto    config     = f.base();
    config.system_info = nullptr; // nothing about the system: no resource log, no disk in the debug info
    f.runtime          = std::make_unique<PlayerRuntimeApi>(f.fs, f.clock, f.transport, config);

    CHECK(f.runtime->poll().empty());
    (void)f.runtime->nextDeadline();
    f.runtime->mediaFinished(1);
    f.runtime->mediaFailed(1);
    f.runtime->elementActivated(1);
    f.runtime->accessKey('k');
    f.runtime->notify("door");
    f.runtime->screenshotTaken("shot.jpg");
    f.runtime->cachesCleared();
    f.runtime->stop();
    CHECK(f.runtime->poll().empty());

    DebugInfo info = f.runtime->debugInfo();
    CHECK(info.index_title.empty());
    CHECK_FALSE(info.disk_total.has_value());

    PlayerSettings next = f.runtime->playerSettings();
    next.player_name    = "lobby";
    next.index_uri      = "http://hub/index.smil";
    next.httpd_enabled  = false;
    CHECK(f.runtime->changePlayerSettings(next).has_value());
    CHECK(f.transport.requests.empty());
}

TEST_CASE("a download that cannot start fails at the next poll and does not keep the player busy")
{
    Fixture f;
    f.transport.refuse_start = true;
    REQUIRE(f.runtime->start().has_value());
    f.run(1s);
    REQUIRE_FALSE(f.transport.requests.empty());
    auto next = f.runtime->nextDeadline();
    REQUIRE(next.has_value());
    CHECK(*next - f.clock.monotonic() > PlayerRuntimeApi::busy_poll);
}

TEST_CASE("start: a task that waited for the restart is completed")
{
    Fixture f("[httpd]\nenabled=false\n[tasks]\npending_id=r1\n");
    REQUIRE(f.runtime->start().has_value());
    std::string task_log = f.fs.content("logs/task_execution_log.xml").value_or("");
    CHECK(task_log.contains("<task id=\"r1\">"));
    CHECK(task_log.contains("<state>completed</state>"));
}

TEST_CASE("resources are written to debug.log every 5 minutes")
{
    Fixture f;
    auto    config     = f.base();
    config.system_info = []
    {
        reports::SystemInfo system;
        system.total_capacity = 1000;
        system.free_space     = 400;
        system.memory_total   = 2048;
        system.memory_used    = 512;
        return system;
    };
    f.runtime = std::make_unique<PlayerRuntimeApi>(f.fs, f.clock, f.transport, config);
    REQUIRE(f.runtime->start().has_value());
    f.run(299s);
    CHECK_FALSE(f.debugLog().contains(" disc "));
    f.run(2s);
    CHECK(f.debugLog().contains("disc 400/1000 memory 512/2048"));
}

TEST_CASE("stop(): StopMedia comes with the next poll, nextDeadline is at once")
{
    Fixture f;
    f.serve("http://hub/index.smil", indexText(R"x(<img xml:id="a" src="a.jpg" dur="indefinite"/>)x", 3600),
            "application/smil+xml");
    f.serve("http://hub/a.jpg", "a", "image/jpeg");
    REQUIRE(f.runtime->start().has_value());
    f.run(5s);
    REQUIRE(f.seen() == "layout No Title | start a");

    f.runtime->stop();
    CHECK(f.runtime->nextDeadline() == f.clock.monotonic());
    for (const CoreEvent &event : f.runtime->poll())
        f.record(event);
    CHECK(f.seen() == "layout No Title | start a | stop");
}

TEST_CASE("nextDeadline: the next change of the engine, the next upload of a report")
{
    Fixture f;
    f.serve("http://hub/index.smil", indexText(R"x(<img xml:id="a" src="a.jpg" dur="10s"/>)x", 3600), "application/smil+xml");
    f.serve("http://hub/a.jpg", "a", "image/jpeg");
    REQUIRE(f.runtime->start().has_value());
    f.run(1s);
    REQUIRE(f.seen() == "layout No Title | start a");
    auto next = f.runtime->nextDeadline();
    REQUIRE(next.has_value());
    CHECK(*next <= f.clock.monotonic() + 10s); // the end of a

    Fixture r;
    r.serve("http://hub/index.smil", indexText("", 3600, R"x(<metadata><subscription><type>SystemReport</type>
            <action>http://hub/system.xml</action><refreshInterval>20</refreshInterval></subscription></metadata>)x"),
            "application/smil+xml");
    REQUIRE(r.runtime->start().has_value());
    r.run(1s);
    REQUIRE(r.transport.activeCount() == 0); // the upload is done
    next = r.runtime->nextDeadline();
    REQUIRE(next.has_value());
    CHECK(*next <= r.clock.monotonic() + 20s);
}

TEST_CASE("nextDeadline: fast while a report is uploaded (curl needs poll())")
{
    Fixture f;
    f.serve("http://hub/index.smil", indexText("", 3600, R"x(<metadata><subscription><type>SystemReport</type>
            <action>http://hub/system.xml</action><refreshInterval>20</refreshInterval></subscription></metadata>)x"),
            "application/smil+xml");
    REQUIRE(f.runtime->start().has_value());
    auto uploading = [&]
    {
        return std::ranges::any_of(f.transport.requests, [](const auto &r) { return r.method == downloader::HttpMethod::Put; }) &&
               f.transport.activeCount() == 1;
    };
    for (int i = 0; i < 100 && !uploading(); ++i)
        (void)f.runtime->poll();
    REQUIRE(uploading()); // the PUT of the system report runs, the downloads are done
    auto next = f.runtime->nextDeadline();
    REQUIRE(next.has_value());
    CHECK(*next <= f.clock.monotonic() + PlayerRuntimeApi::busy_poll);

    f.run(1s); // polled until it is done
    CHECK(f.transport.activeCount() == 0);
}

// ------------------------------------------------------------------ input of the frontend, the engine

TEST_CASE("input of the frontend: a failed medium is skipped, a touch and a notify start their elements")
{
    Fixture f;
    f.serve("http://hub/index.smil", indexText(R"x(<par begin="2s">
                <seq><video xml:id="v" src="v.mp4"/><img xml:id="i" src="i.jpg" dur="indefinite"/></seq>
                <img xml:id="t" src="t.jpg" begin="i.activateEvent" dur="indefinite"/>
                <img xml:id="n" src="n.jpg" begin="notify(door)" dur="indefinite"/></par>)x", 3600),
            "application/smil+xml");
    for (std::string name : {"v.mp4", "i.jpg", "t.jpg", "n.jpg"})
        f.serve("http://hub/" + name, name, name.ends_with("mp4") ? "video/mp4" : "image/jpeg");
    REQUIRE(f.runtime->start().has_value());
    f.run(5s); // the par begins after 2 s, when the media are there
    REQUIRE(f.seen() == "layout No Title | start v");

    f.runtime->mediaFailed(f.instances["v"]);
    f.run(100ms);
    CHECK(f.seen().ends_with("start i"));
    f.runtime->elementActivated(f.instances["i"]);
    f.run(100ms);
    CHECK(f.seen().ends_with("start t"));
    f.runtime->notify("door");
    f.run(100ms);
    CHECK(f.seen().ends_with("start n"));
}

TEST_CASE("smil-playerId() and smil-playerName() in expr come from the configuration")
{
    Fixture f("[General]\nuuid=u-1\nplayer_name=Lobby\n[httpd]\nenabled=false\n");
    f.serve("http://hub/index.smil", indexText(R"x(<par begin="2s"><img xml:id="a" src="a.jpg" dur="indefinite" expr="smil-playerName()='Lobby'"/>
                                                    <img xml:id="b" src="a.jpg" dur="indefinite" expr="smil-playerId()='u-1'"/></par>)x", 3600),
            "application/smil+xml");
    f.serve("http://hub/a.jpg", "a", "image/jpeg");
    REQUIRE(f.runtime->start().has_value());
    f.run(5s);
    CHECK(f.seen().contains("start a"));
    CHECK(f.seen().contains("start b"));
}

TEST_CASE("random_seed goes to the engine: another seed, another order")
{
    auto order = [](std::uint32_t seed)
    {
        Fixture f;
        auto    config  = f.base();
        config.random_seed = seed;
        f.runtime       = std::make_unique<PlayerRuntimeApi>(f.fs, f.clock, f.transport, config);
        std::string body = "<seq><metadata/>";
        for (char c = 'a'; c <= 'h'; ++c)
        {
            body += std::string("<img xml:id=\"") + c + "\" src=\"a.jpg\" dur=\"1s\"/>";
        }
        f.serve("http://hub/index.smil", indexText(body + "</seq>", 3600), "application/smil+xml");
        f.serve("http://hub/a.jpg", "a", "image/jpeg");
        REQUIRE(f.runtime->start().has_value());
        f.run(30s);
        return f.seen();
    };
    CHECK(order(1) == order(1));
    CHECK(order(1) != order(2));
}

// ------------------------------------------------------------------ tasks

TEST_CASE("updateSettings from the hub: the new index URI is loaded and the restart is planned")
{
    TimeZone tz("UTC"); // the FakeClock starts on Thursday, 2026-01-01 00:00
    Fixture  f;
    f.serve("http://hub/index.smil", indexText("", 3600,
                                          R"x(<metadata><subscription><type>TaskSchedule</type><action>http://hub/tasks.xml</action></subscription></metadata>)x"),
            "application/smil+xml");
    f.serve("http://hub/tasks.xml", R"x(<s><timedTaskList><updateSettings id="s1"><sourceURI>http://hub/config.xml</sourceURI>
                                        </updateSettings></timedTaskList></s>)x",
            "application/xml");
    f.serve("http://hub/config.xml", R"x(<configuration><prop name="content.serverUrl" value="http://hub/other.smil"/>
                                         <prop name="schedule.reboot.days" value="thursday"/>
                                         <prop name="schedule.reboot.time" value="00:10:00"/></configuration>)x",
            "application/xml");
    f.serve("http://hub/other.smil", indexText("", 3600), "application/smil+xml");
    REQUIRE(f.runtime->start().has_value());
    f.run(5s);
    CHECK(std::ranges::contains(f.transport.requests, std::string("http://hub/other.smil"), &downloader::HttpRequest::url));

    f.run(10min);
    CHECK(f.seen().contains("reboot DailyReboot"));
}

TEST_CASE("updateSettings from the hub with restart days the player does not know: the planned restart is dropped")
{
    TimeZone tz("UTC"); // the FakeClock starts on Thursday, 2026-01-01 00:00
    Fixture  f("[httpd]\nenabled=false\n[schedule]\nreboot_days=thursday\nreboot_time=00:10:00\n");
    f.serve("http://hub/index.smil", indexText("", 3600,
                                          R"x(<metadata><subscription><type>TaskSchedule</type><action>http://hub/tasks.xml</action></subscription></metadata>)x"),
            "application/smil+xml");
    f.serve("http://hub/tasks.xml", R"x(<s><timedTaskList><updateSettings id="s1"><sourceURI>http://hub/config.xml</sourceURI>
                                        </updateSettings></timedTaskList></s>)x",
            "application/xml");
    f.serve("http://hub/config.xml", R"x(<configuration><prop name="schedule.reboot.days" value="none"/></configuration>)x",
            "application/xml");
    REQUIRE(f.runtime->start().has_value());
    f.run(5s);
    REQUIRE(f.runtime->configuration().rebootDays() == "none");

    f.run(15min);
    CHECK_FALSE(f.seen().contains("reboot DailyReboot"));
}

TEST_CASE("without a supervisor an installation is only written to debug.log")
{
    Fixture f;
    auto    config = f.base();
    config.supervisor = nullptr;
    f.runtime         = std::make_unique<PlayerRuntimeApi>(f.fs, f.clock, f.transport, config);
    f.serve("http://hub/index.smil", indexText("", 3600,
                                          R"x(<metadata><subscription><type>TaskSchedule</type><action>http://hub/tasks.xml</action></subscription></metadata>)x"),
            "application/smil+xml");
    f.serve("http://hub/fw", "binary", "application/octet-stream");
    f.serve("http://hub/tasks.xml", R"x(<s><timedTaskList><firmwareUpdate id="f1"><sourceURI>http://hub/fw</sourceURI>
                                        <version>2.1</version></firmwareUpdate></timedTaskList></s>)x",
            "application/xml");
    REQUIRE(f.runtime->start().has_value());
    f.run(5s);
    CHECK(f.debugLog().contains("none, software not installed: cache/garlic-player."));
}

TEST_CASE("clear_webcache: the player cache stays, the restart comes after the frontend")
{
    Fixture f;
    f.serve("http://hub/index.smil", indexText("", 3600,
                                          R"x(<metadata><subscription><type>TaskSchedule</type><action>http://hub/tasks.xml</action></subscription></metadata>)x"),
            "application/smil+xml");
    f.serve("http://hub/tasks.xml", R"x(<s><timedTaskList><applyCommand id="w1"><command>clear_webcache</command></applyCommand>
                                        </timedTaskList></s>)x",
            "application/xml");
    REQUIRE(f.runtime->start().has_value());
    f.run(5s);
    CHECK(f.seen().ends_with("clear  web"));
    CHECK(f.fs.exists("cache/index.smil"));
    f.runtime->cachesCleared();
    CHECK(f.seen().ends_with("reboot w1"));
}

// ------------------------------------------------------------------ settings, debug info

TEST_CASE("debug info: more memory used than there is gives 0 free")
{
    Fixture f;
    auto    config = f.base();
    config.system_info = []
    {
        reports::SystemInfo system;
        system.memory_total = 1000;
        system.memory_used  = 3000;
        return system;
    };
    f.runtime = std::make_unique<PlayerRuntimeApi>(f.fs, f.clock, f.transport, config);
    REQUIRE(f.runtime->start().has_value());
    CHECK(f.runtime->debugInfo().memory_free == 0u);
}

TEST_CASE("debug info: without total or used memory nothing is free")
{
    for (bool total : {true, false})
    {
        CAPTURE(total);
        Fixture f;
        auto    config = f.base();
        config.system_info = [total]
        {
            reports::SystemInfo system;
            (total ? system.memory_total : system.memory_used) = 1000;
            return system;
        };
        f.runtime = std::make_unique<PlayerRuntimeApi>(f.fs, f.clock, f.transport, config);
        REQUIRE(f.runtime->start().has_value());
        CHECK_FALSE(f.runtime->debugInfo().memory_free.has_value());
    }
}

// ------------------------------------------------------------------ the REST API through the HTTP server

namespace
{
    std::uint16_t freePort(common::FileSystem &fs)
    {
        httpd::Router        router;
        httpd::MhdServer     server(router, fs);
        httpd::HttpdSettings settings;
        settings.port         = 0;
        settings.bind_address = "127.0.0.1";
        REQUIRE(server.start(settings).has_value());
        return server.port();
    }

    std::string request(const std::string &method, const std::string &path, const std::string &content_type = "",
                        const std::string &body = "")
    {
        return method + " " + path + " HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n" +
               (content_type.empty() ? "" : "Content-Type: " + content_type + "\r\n") +
               "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    }

    std::string tokenOf(const std::string &response)
    {
        std::string body  = tests::bodyOf(response);
        auto        start = body.find("\"access_token\":\"");
        REQUIRE(start != std::string::npos);
        start += 16;
        return body.substr(start, body.find('"', start) - start);
    }
}

TEST_CASE("REST API: notify, reboot, switch, exec and screenshots reach the runtime")
{
    TimeZone      tz("UTC");
    Fixture       probe;
    std::uint16_t port = freePort(probe.fs);
    Fixture       f("[httpd]\nenabled=true\nhost=127.0.0.1\nport=" + std::to_string(port) + "\n");
    f.serve("http://hub/index.smil", indexText(R"x(<par begin="2s"><img xml:id="a" src="a.jpg" dur="indefinite"/>
                                                    <img xml:id="n" src="a.jpg" begin="notify(door)" dur="indefinite"/></par>)x", 3600),
            "application/smil+xml");
    f.serve("http://hub/other.smil", indexText("", 3600), "application/smil+xml");
    f.serve("http://hub/a.jpg", "a", "image/jpeg");
    REQUIRE(f.runtime->start().has_value());
    f.run(5s);
    REQUIRE(f.seen() == "layout No Title | start a");

    std::string token = tokenOf(f.http(port, request("POST", "/v2/oauth2/token", "application/x-www-form-urlencoded",
                                                     "grant_type=password&username=admin&password=")));
    std::string auth  = "?access_token=" + token;

    f.http(port, request("POST", "/v2/task/notify" + auth + "&smilEvent=door"));
    f.run(100ms);
    CHECK(f.seen().ends_with("start n"));

    f.http(port, request("POST", "/v2/task/reboot" + auth));
    CHECK(f.seen().contains("reboot httpd: 2026-01-01T00:00:"));

    auto index_requests = [&] { return std::ranges::count(f.transport.requests, std::string("http://hub/index.smil"), &downloader::HttpRequest::url); };
    auto before         = index_requests();
    f.http(port, request("POST", "/v2/app/switch" + auth + "&mode=start"));
    f.run(1s);
    CHECK(index_requests() > before);

    f.http(port, request("POST", "/v2/app/exec" + auth + "&uri=http://hub/other.smil"));
    f.run(1s);
    CHECK(std::ranges::contains(f.transport.requests, std::string("http://hub/other.smil"), &downloader::HttpRequest::url));
    CHECK(f.runtime->configuration().indexUri() == "http://hub/index.smil"); // exec plays it only for a while

    // a screenshot: the frontend gets the event, the answer waits for screenshotTaken()
    auto shot = std::async(std::launch::async, [port, auth] { return tests::sendRaw(port, request("GET", "/v2/task/screenshot" + auth)); });
    auto end  = std::chrono::steady_clock::now() + 5s;
    while (!f.seen().contains("screenshot ") && std::chrono::steady_clock::now() < end)
        for (const CoreEvent &event : f.runtime->poll())
            f.record(event);
    REQUIRE(f.seen().contains("screenshot "));
    std::string path = f.log.back().substr(11);
    f.write(path, "JPEG");
    f.runtime->screenshotTaken(path);
    while (shot.wait_for(1ms) != std::future_status::ready && std::chrono::steady_clock::now() < end)
        f.runtime->poll();
    REQUIRE(shot.wait_for(0s) == std::future_status::ready);
    CHECK(tests::bodyOf(shot.get()) == "JPEG");

    // no screenshot within 5 s: the text of garlic-lib (a second later: another file name)
    f.clock.advance(1s);
    auto late = f.http(port, request("GET", "/v2/task/screenshot" + auth), 100ms);
    CHECK(tests::bodyOf(late) == "file not created.");
}

TEST_CASE("REST API: an upload without room in the cache gets the error of garlic-lib")
{
    Fixture       probe;
    std::uint16_t port = freePort(probe.fs);
    Fixture       f("[httpd]\nenabled=true\nhost=127.0.0.1\nport=" + std::to_string(port) + "\n");
    auto          config = f.base();
    config.free_space    = [] { return std::uint64_t{0}; };
    f.runtime            = std::make_unique<PlayerRuntimeApi>(f.fs, f.clock, f.transport, config);
    REQUIRE(f.runtime->start().has_value());

    std::string token = tokenOf(f.http(port, request("POST", "/v2/oauth2/token", "application/x-www-form-urlencoded",
                                                     "grant_type=password&username=admin&password=")));
    std::string body  = "--XyZ123\r\nContent-Disposition: form-data; name=\"downloadPath\"\r\n\r\nhttp://x/a.jpg\r\n"
                        "--XyZ123\r\nContent-Disposition: form-data; name=\"fileSize\"\r\n\r\n100\r\n"
                        "--XyZ123\r\nContent-Disposition: form-data; name=\"data\"; filename=\"a.jpg\"\r\n\r\nimage\r\n"
                        "--XyZ123--\r\n";
    auto response = f.http(port, request("POST", "/v2/files/new?access_token=" + token,
                                         "multipart/form-data; boundary=XyZ123", body));
    CHECK(tests::bodyOf(response).contains("Neccessary 110 byte cannot be relesed from disc space"));
}

TEST_CASE("the HTTP server: a port that is in use is written to debug.log, a new port restarts it")
{
    Fixture       probe;
    std::uint16_t first = freePort(probe.fs);
    httpd::Router        router;
    httpd::MhdServer     blocker(router, probe.fs);
    httpd::HttpdSettings blocking;
    blocking.port = first;
    REQUIRE(blocker.start(blocking).has_value());

    Fixture f("[httpd]\nenabled=true\nhost=127.0.0.1\nport=" + std::to_string(first) + "\n");
    REQUIRE(f.runtime->start().has_value());
    CHECK(f.debugLog().contains("not started"));
    blocker.stop();

    // on and running on a free port, then moved to another one
    PlayerSettings next = f.runtime->playerSettings();
    next.httpd_port     = freePort(probe.fs);
    next.httpd_enabled  = true;
    REQUIRE(f.runtime->changePlayerSettings(next).has_value());
    CHECK(tests::statusOf(f.http(next.httpd_port, request("GET", "/nothing"))) == 404);

    std::uint16_t old = next.httpd_port;
    next.httpd_port   = freePort(probe.fs);
    REQUIRE(f.runtime->changePlayerSettings(next).has_value());
    CHECK(tests::statusOf(f.http(next.httpd_port, request("GET", "/nothing"))) == 404);
    CHECK(tests::sendRaw(old, request("GET", "/nothing")) == "connect failed");

    // only the name changes: the server runs on, it is not started a second time on its own port
    next.player_name = "lobby";
    REQUIRE(f.runtime->changePlayerSettings(next).has_value());
    CHECK(tests::statusOf(f.http(next.httpd_port, request("GET", "/nothing"))) == 404);
}

// ------------------------------------------------------------------ launcher

TEST_CASE("a configuration.xml of the launcher at runtime: applied, user agent and index follow; missing: false")
{
    Fixture f;
    f.serve("http://hub/index.smil", indexText(R"x(<img xml:id="a" src="img/a.jpg" dur="10s"/>)x", 60),
            "application/smil+xml");
    f.serve("http://hub/img/a.jpg", "jpeg", "image/jpeg");
    REQUIRE(f.runtime->start().has_value());
    f.run(1s);

    f.write("usb/config.xml", R"(<configuration><prop name="info.playerName" value="Lobby"/>
        <prop name="content.serverUrl" value="http://hub/usb.smil"/></configuration>)");
    CHECK(f.runtime->applyConfigurationFile("usb/config.xml"));
    f.run(1s);

    CHECK(f.runtime->configuration().playerName() == "Lobby");
    CHECK(f.runtime->configuration().indexUri() == "http://hub/usb.smil");
    auto request = std::ranges::find(f.transport.requests, std::string("http://hub/usb.smil"), &downloader::HttpRequest::url);
    REQUIRE(request != f.transport.requests.end());
    CHECK(request->headers.find("User-Agent").value_or("").find("NAME:Lobby") != std::string::npos);

    CHECK_FALSE(f.runtime->applyConfigurationFile("usb/missing.xml"));
    CHECK(f.runtime->configuration().indexUri() == "http://hub/usb.smil");
}

TEST_CASE("remainingTime: the rest of the dur of a playing instance, rounded up; nothing without a planned end")
{
    Fixture f;
    f.serve("http://hub/index.smil",
            indexText(R"x(<par><img xml:id="a" src="img/a.jpg" dur="10s"/><img xml:id="b" src="img/b.jpg" dur="indefinite"/></par>)x", 60),
            "application/smil+xml");
    f.serve("http://hub/img/a.jpg", "jpeg", "image/jpeg");
    f.serve("http://hub/img/b.jpg", "jpeg", "image/jpeg");
    CHECK_FALSE(f.runtime->remainingTime(1).has_value()); // before start(): no engine
    REQUIRE(f.runtime->start().has_value());
    f.run(1s);
    REQUIRE(f.instances.contains("a"));

    f.clock.advance(2500ms);
    auto rest = f.runtime->remainingTime(f.instances["a"]);
    REQUIRE(rest.has_value());
    CHECK(*rest > 5s); // the download took a moment of the second
    CHECK(*rest <= 8s);
    CHECK_FALSE(f.runtime->remainingTime(f.instances["b"]).has_value());
}

TEST_CASE("uuid of the supervisor: taken at the first start, a stored uuid stays")
{
    Fixture f; // its ini has no uuid
    auto config = f.base();
    config.uuid = "11111111-2222-4333-8444-555555555555";
    f.runtime   = std::make_unique<PlayerRuntimeApi>(f.fs, f.clock, f.transport, config);
    REQUIRE(f.runtime->start().has_value());
    CHECK(f.runtime->configuration().uuid() == "11111111-2222-4333-8444-555555555555");

    f.runtime.reset();
    config.uuid = "99999999-2222-4333-8444-555555555555";
    f.runtime   = std::make_unique<PlayerRuntimeApi>(f.fs, f.clock, f.transport, config);
    REQUIRE(f.runtime->start().has_value());
    CHECK(f.runtime->configuration().uuid() == "11111111-2222-4333-8444-555555555555");
}
