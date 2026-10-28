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
#include "reports/reporter.hpp"

using namespace garlic::core;
using namespace garlic::core::reports;
using garlic::core::tests::FakeClock;
using namespace std::chrono_literals;

namespace
{
    struct Fixture
    {
        FakeClock                             clock;
        common::MemoryFileSystem              fs{clock};
        downloader::tests::FakeHttpTransport  transport;
        logger::Logger                        logger{fs, clock, loggerConfig()};
        configuration::Configuration          config{fs, clock, "config/garlic-player.ini"};
        database::InventoryTable              inventory{clock};
        Reporter reporter{transport, fs, clock, logger, config, inventory, "cache", [] { return SystemInfo{}; }};

        Fixture()
        {
            REQUIRE(config.load().has_value());
            REQUIRE(inventory.open(":memory:").has_value());
            answer(201);
        }

        static logger::LoggerConfig loggerConfig()
        {
            logger::LoggerConfig c;
            c.log_dir = "logs";
            return c;
        }

        void answer(int status)
        {
            downloader::tests::FakeHttpTransport::Response r;
            r.status         = status;
            transport.fallback = r;
        }

        /// Polls like the host until nothing is running.
        void run()
        {
            for (int i = 0; i < 100; ++i)
            {
                reporter.poll();
                transport.poll();
                if (transport.activeCount() == 0 && (!reporter.nextDeadline() || *reporter.nextDeadline() > clock.monotonic()))
                    return;
            }
        }

        void wait(std::chrono::seconds time)
        {
            clock.advance(time);
            run();
        }

        std::vector<std::string> puts() const
        {
            std::vector<std::string> urls;
            for (const auto &request : transport.requests)
                if (request.method == downloader::HttpMethod::Put)
                    urls.push_back(request.url);
            return urls;
        }
    };

    smil::Subscription subscription(std::string type, std::string action, int refresh = 0, int retry = 0)
    {
        return {std::move(type), std::move(action), "put", refresh, retry};
    }
}

TEST_CASE("system and inventory report: PUT to the action at once and every refreshInterval")
{
    Fixture f;
    f.reporter.subscribe({subscription("SystemReport", "http://hub/system.xml", 60),
                          subscription("InventoryReport", "http://hub/inventory.xml", 120), subscription("TaskSchedule", "x")});
    f.run();
    CHECK(f.puts() == std::vector<std::string>{"http://hub/system.xml", "http://hub/inventory.xml"});
    const auto &request = f.transport.requests[0];
    CHECK(request.headers.find("Content-Type") == "application/xml");
    CHECK(request.body.find("<player id=\"" + f.config.uuid() + "\">") != std::string::npos);

    f.wait(60s);
    CHECK(f.puts().size() == 3);
    f.wait(60s);
    CHECK(f.puts().size() == 5);
}

TEST_CASE("without refreshInterval: only once (garlic-lib sent all the time)")
{
    Fixture f;
    f.reporter.subscribe({subscription("SystemReport", "http://hub/system.xml")});
    f.run();
    f.wait(3600s);
    CHECK(f.puts().size() == 1);
    CHECK_FALSE(f.reporter.nextDeadline().has_value());
}

TEST_CASE("after a failure: retryInterval, without it the next refresh")
{
    Fixture f;
    f.answer(500);
    f.reporter.subscribe({subscription("SystemReport", "http://hub/a.xml", 300, 30),
                          subscription("InventoryReport", "http://hub/b.xml", 300)});
    f.run();
    CHECK(f.puts().size() == 2);

    f.wait(30s);
    CHECK(f.puts().size() == 3); // the system report again after 30 s
    CHECK(f.puts().back() == "http://hub/a.xml");
    f.answer(201);
    f.wait(30s);
    CHECK(f.puts().size() == 4);
    f.wait(240s); // 300 s: the inventory report with its refresh
    CHECK(f.puts().back() == "http://hub/b.xml");
}

TEST_CASE("play logs: current one rotated, oldest first, one file per upload with an own name, deleted after")
{
    Fixture f;
    f.logger.played("first", f.clock.now());
    f.logger.rotate(logger::LogFile::Play); // an older file
    f.logger.played("second", f.clock.now());

    f.reporter.subscribe({subscription("PlaylogCollection", "http://hub/logs", 60)});
    f.run();

    auto puts = f.puts();
    REQUIRE(puts.size() == 2);
    CHECK(puts[0].starts_with("http://hub/logs/playlog-"));
    CHECK(puts[0].ends_with(f.config.uuid().substr(23) + ".xml"));
    CHECK(puts[0] != puts[1]);
    CHECK(f.transport.requests[0].body.find("<contentPlayLog><contentPlayed><contentId>first") != std::string::npos);
    CHECK(f.transport.requests[1].body.find("<contentId>second") != std::string::npos);
    CHECK_FALSE(f.fs.exists("logs/play_log.xml"));
    CHECK_FALSE(f.fs.exists("logs/play_log.xml.1"));
    CHECK_FALSE(f.fs.exists("logs/play_log.xml.sending"));
}

TEST_CASE("a failed log upload keeps the file and sends it first next time, new entries are not lost")
{
    Fixture f;
    f.answer(503);
    f.logger.event(logger::Severity::Informational, "System", "FIRST");
    f.reporter.subscribe({subscription("EventlogCollection", "http://hub/logs", 60, 10)});
    f.run();
    CHECK(f.fs.exists("logs/event_log.xml.sending"));

    f.logger.event(logger::Severity::Informational, "System", "SECOND"); // meanwhile
    f.answer(201);
    f.wait(10s);
    auto puts = f.puts();
    REQUIRE(puts.size() == 3); // failed, the kept file again, then the new one
    CHECK(puts[1].starts_with("http://hub/logs/event-"));
    CHECK(f.transport.requests[1].body.find("FIRST") != std::string::npos);
    CHECK(f.transport.requests[2].body.find("SECOND") != std::string::npos);
    CHECK_FALSE(f.fs.exists("logs/event_log.xml.sending"));
}

TEST_CASE("inventory report: TRANSFER and COMPLETE with the bytes in the cache")
{
    Fixture f;
    downloader::InventoryEntry done{"http://hub/a.jpg", "abc.jpg", "", "", "image/jpeg", 4,
                                    downloader::InventoryState::Complete};
    downloader::InventoryEntry running{"http://hub/b.mp4", "def.mp4", "", "", "video/mp4", 10,
                                       downloader::InventoryState::Transfer};
    f.inventory.save(done);
    f.inventory.save(running);
    auto write = [&](const std::string &path, std::string_view text)
    {
        auto file = f.fs.openForWrite(path, common::WriteMode::Truncate);
        REQUIRE(file.has_value());
        REQUIRE((*file)->write(std::as_bytes(std::span(text.data(), text.size()))).has_value());
        REQUIRE((*file)->close().has_value());
    };
    write("cache/abc.jpg", "jpeg");
    write("cache/def.mp4.part", "vid");

    f.reporter.subscribe({subscription("InventoryReport", "http://hub/inventory.xml")});
    f.run();
    const std::string &body = f.transport.requests.at(0).body;
    CHECK(body.find("<object id=\"abc\" persistent=\"false\"><resourceURI>http://hub/a.jpg</resourceURI>"
                    "<state>COMPLETE</state>") != std::string::npos);
    CHECK(body.find("<transferLength>4</transferLength>") != std::string::npos);
    CHECK(body.find("<state>TRANSFER</state>") != std::string::npos); // garlic-lib said COMPLETE
    CHECK(body.find("<transferLength>3</transferLength>") != std::string::npos);
}

TEST_CASE("an unchanged subscription keeps its schedule, a changed one sends at once, a removed one stops")
{
    Fixture f;
    f.reporter.subscribe({subscription("SystemReport", "http://hub/system.xml", 60)});
    f.run();
    f.wait(10s);
    f.reporter.subscribe({subscription("SystemReport", "http://hub/system.xml", 60)}); // same index again
    f.run();
    CHECK(f.puts().size() == 1);

    f.reporter.subscribe({subscription("SystemReport", "http://hub/other.xml", 60)});
    f.run();
    CHECK(f.puts().back() == "http://hub/other.xml");

    f.reporter.subscribe({});
    f.wait(120s);
    CHECK(f.puts().size() == 2);
}
