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

TEST_CASE("task execution report: the last state of every task from all task logs, the log is kept")
{
    Fixture f;
    f.logger.taskState("reboot-1", "pending");
    f.logger.taskState("settings-1", "aborted");
    f.logger.rotate(logger::LogFile::TaskExecution); // an older file
    f.clock.advance(60s);
    f.logger.taskState("reboot-1", "completed");
    f.logger.taskState("a&b", "completed");

    f.reporter.subscribe({subscription("TaskExecutionReport", "http://hub/task_execution-uuid.xml", 60)});
    f.run();
    REQUIRE(f.puts() == std::vector<std::string>{"http://hub/task_execution-uuid.xml"});

    const std::string &body = f.transport.requests[0].body;
    auto task = [&](std::string_view id) { return body.find("<task id=\"" + std::string(id) + "\">"); };
    CHECK(body.find("<player id=\"" + f.config.uuid() + "\"><taskState><task id=\"reboot-1\">") != std::string::npos);
    CHECK(task("reboot-1") < task("settings-1")); // order of the first state
    CHECK(task("settings-1") < task("a&amp;b"));
    CHECK(body.find("<state>pending</state>") == std::string::npos); // replaced by completed
    CHECK(body.find("<state>aborted</state>") != std::string::npos);
    CHECK(body.find("</task></taskState></player>") != std::string::npos);
    CHECK(f.fs.exists("logs/task_execution_log.xml"));
    CHECK(f.fs.exists("logs/task_execution_log.xml.1"));

    f.wait(60s); // the same states again
    REQUIRE(f.puts().size() == 2);
    CHECK(f.transport.requests[1].body.find("<task id=\"settings-1\">") != std::string::npos);
}

TEST_CASE("task execution report without task log: an empty taskState")
{
    Fixture f;
    f.reporter.subscribe({subscription("TaskExecutionReport", "http://hub/task_execution-uuid.xml")});
    f.run();
    REQUIRE(f.puts().size() == 1);
    CHECK(f.transport.requests[0].body.find("<taskState></taskState>") != std::string::npos);
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

TEST_CASE("an upload counts as done with 2xx only")
{
    for (int status : {200, 299, 199, 300})
    {
        CAPTURE(status);
        Fixture f;
        f.answer(status);
        f.reporter.subscribe({subscription("SystemReport", "http://hub/a.xml", 0, 10)});
        f.run();
        f.wait(10s); // a failed one is sent again after retryInterval
        CHECK(f.puts().size() == (status / 100 == 2 ? 1u : 2u));
    }
}

TEST_CASE("a subscription with another refresh, retry or method counts as changed: sent at once")
{
    Fixture f;
    f.reporter.subscribe({subscription("SystemReport", "http://hub/a.xml", 60, 10)});
    f.run();

    f.reporter.subscribe({subscription("SystemReport", "http://hub/a.xml", 120, 10)});
    f.run();
    CHECK(f.puts().size() == 2);
    f.reporter.subscribe({subscription("SystemReport", "http://hub/a.xml", 120, 20)});
    f.run();
    CHECK(f.puts().size() == 3);
    f.reporter.subscribe({{"SystemReport", "http://hub/a.xml", "post", 120, 20}});
    f.run();
    CHECK(f.puts().size() == 4);
}

TEST_CASE("the same report type twice: the last subscription counts")
{
    Fixture f;
    f.reporter.subscribe({subscription("SystemReport", "http://hub/a.xml"), subscription("SystemReport", "http://hub/b.xml")});
    f.run();
    CHECK(f.puts() == std::vector<std::string>{"http://hub/b.xml"});
}

TEST_CASE("a running upload is cancelled when its subscription goes or the reporter is destroyed")
{
    Fixture f;
    f.reporter.subscribe({subscription("SystemReport", "http://hub/a.xml")});
    f.reporter.poll(); // started, not answered yet
    REQUIRE(f.transport.activeCount() == 1);
    f.reporter.subscribe({});
    CHECK(f.transport.activeCount() == 0);

    {
        Reporter other{f.transport, f.fs, f.clock, f.logger, f.config, f.inventory, "cache", nullptr};
        other.subscribe({subscription("SystemReport", "http://hub/a.xml")});
        other.poll();
        REQUIRE(f.transport.activeCount() == 1);
    }
    CHECK(f.transport.activeCount() == 0);
}

TEST_CASE("while an upload runs it is not started again and has no deadline")
{
    Fixture f;
    f.reporter.subscribe({subscription("SystemReport", "http://hub/a.xml", 1)});
    f.reporter.poll();
    CHECK_FALSE(f.reporter.nextDeadline().has_value());
    f.clock.advance(5s);
    f.reporter.poll();
    CHECK(f.puts().size() == 1);
}

TEST_CASE("nextDeadline is the earliest report")
{
    Fixture f;
    f.reporter.subscribe({subscription("InventoryReport", "http://hub/b.xml", 120), subscription("SystemReport", "http://hub/a.xml", 60)});
    f.run();
    CHECK(f.reporter.nextDeadline() == f.clock.monotonic() + 60s);
}

TEST_CASE("no log to send: tried again after refreshInterval")
{
    Fixture f;
    f.reporter.subscribe({subscription("PlaylogCollection", "http://hub/logs", 60)});
    f.run();
    CHECK(f.puts().empty());
    CHECK(f.reporter.nextDeadline() == f.clock.monotonic() + 60s);

    f.logger.played("first", f.clock.now());
    f.wait(60s);
    CHECK(f.puts().size() == 1);
}

TEST_CASE("uploads carry the User-Agent of the player")
{
    Fixture f;
    f.reporter.subscribe({subscription("SystemReport", "http://hub/a.xml")});
    f.run();
    CHECK(f.transport.requests.at(0).headers.find("User-Agent") == f.config.userAgent());
}

TEST_CASE("an upload that cannot be started or fails is logged and tried again after retryInterval")
{
    Fixture f;
    f.transport.refuse_start = true;
    f.reporter.subscribe({subscription("SystemReport", "http://hub/a.xml", 0, 10)});
    f.run();
    CHECK(f.puts().size() == 1); // the fake transport records the refused request too
    CHECK(f.transport.activeCount() == 0);
    CHECK(f.fs.content("logs/debug.log").value_or("").find("upload not started") != std::string::npos);

    f.transport.refuse_start = false;
    f.answer(500);
    f.wait(10s);
    CHECK(f.puts().size() == 2);
    CHECK(f.fs.content("logs/debug.log").value_or("").find("upload failed: http://hub/a.xml") != std::string::npos);

    f.answer(201);
    f.wait(10s);
    CHECK(f.puts().size() == 3);
}

TEST_CASE("task execution report: several rotated task logs, the newest state wins")
{
    Fixture f;
    f.logger.taskState("t1", "pending");
    f.logger.rotate(logger::LogFile::TaskExecution); // becomes .2
    f.logger.taskState("t2", "pending");
    f.logger.taskState("t1", "aborted");
    f.logger.rotate(logger::LogFile::TaskExecution); // .1
    f.logger.taskState("t2", "completed");

    f.reporter.subscribe({subscription("TaskExecutionReport", "http://hub/tasks.xml")});
    f.run();
    const std::string &body = f.transport.requests.at(0).body;
    CHECK(body.find("<state>pending</state>") == std::string::npos);
    CHECK(body.find("<task id=\"t1\">") < body.find("<task id=\"t2\">"));
    CHECK(body.find("<state>aborted</state>") < body.find("<state>completed</state>"));
}

TEST_CASE("without a source of system information and with a short uuid nothing breaks")
{
    Fixture f;
    f.config.setValue("uuid", "short");
    Reporter reporter{f.transport, f.fs, f.clock, f.logger, f.config, f.inventory, "cache", nullptr};
    f.logger.played("first", f.clock.now());
    reporter.subscribe({subscription("SystemReport", "http://hub/a.xml"), subscription("InventoryReport", "http://hub/b.xml"),
                        subscription("PlaylogCollection", "http://hub/logs")});
    for (int i = 0; i < 10; ++i)
    {
        reporter.poll();
        f.transport.poll();
    }
    auto puts = f.puts();
    REQUIRE(puts.size() == 3);
    CHECK(puts[2].size() == std::string("http://hub/logs/playlog-").size() + 23 + 4); // only the random part
}
