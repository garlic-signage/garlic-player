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
#include "fake_http_transport.hpp"
#include "player_runtime/threaded_runtime.hpp"

#include <atomic>
#include <future>

using namespace garlic::core;
using namespace std::chrono_literals;

namespace
{
    /// The frontend side: the UI thread waits for the wake and takes the events. Real time: the core thread waits
    /// for the deadlines of the runtime.
    struct Fixture
    {
        common::SystemClock                  clock;
        common::MemoryFileSystem             fs{clock};
        downloader::tests::FakeHttpTransport transport;
        std::mutex                           mutex;
        std::condition_variable              woken;
        int                                  wakes = 0;
        std::vector<std::string>             seen;
        NewLayout                            layout;
        std::unique_ptr<ThreadedRuntime>     runtime;

        explicit Fixture(std::string database = ":memory:")
        {
            write("config/garlic-player.ini", "[httpd]\nenabled=false\n");
            serve("http://hub/index.smil", R"(<smil><head><meta name="title" content="threaded"/></head>
                                              <body repeatCount="indefinite"><img xml:id="a" src="http://hub/a.jpg" dur="10s"/></body></smil>)",
                  "application/smil+xml");
            serve("http://hub/a.jpg", "jpg", "image/jpeg");

            PlayerRuntimeConfig config;
            config.ini_path      = "config/garlic-player.ini";
            config.cache_dir     = "cache";
            config.log_dir       = "logs";
            config.index_uri     = "http://hub/index.smil";
            config.database_path = std::move(database);
            config.system_info   = [] { return reports::SystemInfo{}; };
            config.free_space    = [] { return std::uint64_t{1} << 40; };
            runtime              = std::make_unique<ThreadedRuntime>(fs, clock, transport, config, [this]
            {
                std::lock_guard lock(mutex);
                ++wakes;
                woken.notify_all();
            });
        }

        void write(const std::string &path, std::string_view text)
        {
            auto file = fs.openForWrite(path, common::WriteMode::Truncate);
            REQUIRE(file.has_value());
            REQUIRE((*file)->write(std::as_bytes(std::span(text.data(), text.size()))).has_value());
            REQUIRE((*file)->close().has_value());
        }

        void serve(const std::string &url, std::string body, const std::string &type)
        {
            downloader::tests::FakeHttpTransport::Response r;
            r.headers.add("Content-Type", type);
            r.headers.add("ETag", "\"v1\"");
            r.body       = std::move(body);
            r.chunk_size = 1 << 20;
            transport.respond(downloader::HttpMethod::Head, url, r);
            transport.respond(downloader::HttpMethod::Get, url, r);
        }

        /// Like a UI thread: wait for wakes and take the events until "until" was seen (at most 5 s).
        bool waitFor(const std::string &until)
        {
            auto end = std::chrono::steady_clock::now() + 5s;
            while (std::chrono::steady_clock::now() < end)
            {
                {
                    std::unique_lock lock(mutex);
                    woken.wait_for(lock, 50ms, [&] { return wakes > 0; });
                    wakes = 0;
                }
                for (const CoreEvent &event : runtime->takeEvents())
                {
                    if (const auto *next = std::get_if<NewLayout>(&event))
                    {
                        layout = *next;
                        seen.push_back("layout " + layout.document->head.title);
                    }
                    else if (const auto *start = std::get_if<smil::StartMedia>(&event))
                        seen.push_back("start " + start->media->id); // points into the kept document
                    else if (std::holds_alternative<smil::StopMedia>(event))
                        seen.push_back("stop");
                }
                if (std::ranges::find(seen, until) != seen.end())
                    return true;
            }
            return false;
        }
    };
}

TEST_CASE("threaded: the core runs in its own thread and wakes the frontend for its events")
{
    Fixture f;
    REQUIRE(f.runtime->start().has_value());
    REQUIRE(f.waitFor("start a"));
    CHECK(f.seen == std::vector<std::string>{"layout threaded", "start a"});

    // a call from the frontend runs in the core thread
    std::promise<std::thread::id> where;
    f.runtime->post([&](PlayerRuntimeApi &) { where.set_value(std::this_thread::get_id()); });
    auto core_thread = where.get_future();
    REQUIRE(core_thread.wait_for(5s) == std::future_status::ready);
    CHECK(core_thread.get() != std::this_thread::get_id());

    // stop of the playback via post: the StopMedia come as events
    f.runtime->post([](PlayerRuntimeApi &r) { r.stop(); });
    CHECK(f.waitFor("stop"));
    f.runtime->stop();
}

TEST_CASE("threaded: an error of start() is returned, the thread is ended")
{
    Fixture f("/not/there/garlic.db");
    auto    started = f.runtime->start();
    REQUIRE_FALSE(started.has_value());
    CHECK(started.error().starts_with("database: "));
    f.runtime->post([](PlayerRuntimeApi &) {}); // no thread: nothing happens
    CHECK(f.runtime->takeEvents().empty());
}

TEST_CASE("threaded: stop() without start() and twice is fine, the destructor stops")
{
    Fixture f;
    f.runtime->stop();
    REQUIRE(f.runtime->start().has_value());
    CHECK_FALSE(f.runtime->start().has_value()); // already running
    f.runtime->stop();
    f.runtime->stop();
    REQUIRE(f.runtime->start().has_value()); // again after a stop
    f.runtime.reset();
}
