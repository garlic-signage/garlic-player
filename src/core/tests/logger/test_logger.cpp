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
#include "logger/logger.hpp"
#include "time_zone.hpp"

#include <vector>

using namespace garlic::core::logger;
using garlic::core::common::MemoryFileSystem;
using garlic::core::tests::FakeClock;
using garlic::core::tests::TimeZone;
using namespace std::chrono_literals;

namespace
{
    LoggerConfig config()
    {
        LoggerConfig c;
        c.log_dir = "logs";
        return c;
    }

    struct Fixture
    {
        TimeZone         tz{"UTC"};
        FakeClock        clock;
        MemoryFileSystem fs{clock};
        Logger           logger;

        explicit Fixture(LoggerConfig c = config()) : logger(fs, clock, std::move(c)) {}

        std::string content(LogFile file) const { return fs.content(logger.path(file)).value_or("<missing>"); }
    };
}

TEST_CASE("file names like garlic-lib")
{
    Fixture f;
    CHECK(f.logger.path(LogFile::Debug) == "logs/debug.log");
    CHECK(f.logger.path(LogFile::Event) == "logs/event_log.xml");
    CHECK(f.logger.path(LogFile::Play) == "logs/play_log.xml");
    CHECK(f.logger.path(LogFile::TaskExecution) == "logs/task_execution_log.xml");
}

TEST_CASE("debug.log: time, severity, category, source position and message per line")
{
    Fixture f;
    f.logger.warning("Database", "inventory could not be opened");
    f.clock.advance(1500ms);
    f.logger.info("Develop", "second");

    std::string text = f.content(LogFile::Debug);
    CHECK(text.starts_with("2026-01-01 00:00:00,000 warning Database test_logger.cpp("));
    CHECK(text.find(") inventory could not be opened\n") != std::string::npos);
    CHECK(text.find("2026-01-01 00:00:01,500 informational Develop test_logger.cpp(") != std::string::npos);
}

TEST_CASE("debug.log: entries below the minimum severity are dropped")
{
    auto c               = config();
    c.min_debug_severity = Severity::Warning;
    Fixture f(c);

    f.logger.debug("Develop", "noise");
    f.logger.info("Develop", "noise");
    f.logger.error("Develop", "important");

    std::string text = f.content(LogFile::Debug);
    CHECK(text.find("noise") == std::string::npos);
    CHECK(text.find("error Develop") != std::string::npos);
}

TEST_CASE("event_log.xml in the format of garlic-lib, values escaped")
{
    Fixture f;
    f.logger.event(Severity::Warning, "ContentManager", "MEDIA_UNSUPPORTED",
                   {{"resourceURI", "http://example.com/a.php?x=1&y=<2>"}, {"contentId", "id-1"}});

    CHECK(f.content(LogFile::Event) ==
          "<event><eventType>warning</eventType><eventTime>2026-01-01T00:00:00Z</eventTime>"
          "<eventSource>ContentManager</eventSource><eventName>MEDIA_UNSUPPORTED</eventName><metadata>"
          "<meta name=\"resourceURI\" content=\"http://example.com/a.php?x=1&amp;y=&lt;2&gt;\"/>"
          "<meta name=\"contentId\" content=\"id-1\"/></metadata></event>\n");
}

TEST_CASE("event without metadata keeps the empty metadata element")
{
    Fixture f;
    f.logger.event(Severity::Informational, "System", "PLAYER_STARTED");

    CHECK(f.content(LogFile::Event).find("<eventName>PLAYER_STARTED</eventName><metadata></metadata></event>") !=
          std::string::npos);
}

TEST_CASE("play_log.xml: content with start and end time")
{
    Fixture f;
    auto    start = f.clock.now();
    f.clock.advance(10s);
    f.logger.played("video1", start);

    CHECK(f.content(LogFile::Play) == "<contentPlayed><contentId>video1</contentId><startTime>2026-01-01T00:00:00Z"
                                      "</startTime><endTime>2026-01-01T00:00:10Z</endTime></contentPlayed>\n");
}

TEST_CASE("task_execution_log.xml")
{
    Fixture f;
    f.logger.taskState("task-7", "COMPLETED");

    CHECK(f.content(LogFile::TaskExecution) == "<taskState><task id=\"task-7\"><lastUpdateTime>2026-01-01T00:00:00Z"
                                               "</lastUpdateTime><state>COMPLETED</state></task></taskState>\n");
}

TEST_CASE("a full log file is rotated, the oldest file is deleted")
{
    auto c      = config();
    c.max_size  = 100;
    c.max_files = 2;
    Fixture f(c);

    for (int i = 0; i < 4; ++i)
        f.logger.played("content-" + std::to_string(i), f.clock.now()); // ~130 bytes each: rotates every time

    std::string base = f.logger.path(LogFile::Play);
    CHECK_FALSE(f.fs.exists(base)); // rotated right after the last write
    CHECK(f.fs.content(base + ".1")->find("content-3") != std::string::npos);
    CHECK(f.fs.content(base + ".2")->find("content-2") != std::string::npos);
    CHECK_FALSE(f.fs.exists(base + ".3"));
}

TEST_CASE("rotate() for sending: the current file becomes .1, new entries start a new file")
{
    Fixture f;
    f.logger.event(Severity::Informational, "System", "FIRST");
    f.logger.rotate(LogFile::Event);
    f.logger.event(Severity::Informational, "System", "SECOND");

    std::string base = f.logger.path(LogFile::Event);
    CHECK(f.fs.content(base + ".1")->find("FIRST") != std::string::npos);
    CHECK(f.content(LogFile::Event).find("SECOND") != std::string::npos);
    CHECK(f.content(LogFile::Event).find("FIRST") == std::string::npos);
}

TEST_CASE("mirror receives debug lines and events")
{
    auto                     c = config();
    std::vector<std::string> mirrored;
    c.mirror = [&](Severity s, std::string_view category, std::string_view text)
    { mirrored.push_back(std::string(toString(s)) + "|" + std::string(category) + "|" + std::string(text)); };
    Fixture f(c);

    f.logger.error("Database", "broken");
    f.logger.event(Severity::Critical, "MediaPlayer", "MEDIA_NOT_AVAILABLE");

    REQUIRE(mirrored.size() == 2);
    CHECK(mirrored[0].starts_with("error|Database|2026-01-01 00:00:00,000 error Database"));
    CHECK(mirrored[1].starts_with("critical|MediaPlayer|<event>"));
}
