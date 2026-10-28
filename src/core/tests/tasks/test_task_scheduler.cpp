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

#include "common/md5.hpp"
#include "common/memory_file_system.hpp"
#include "fake_clock.hpp"
#include "fake_http_transport.hpp"
#include "memory_inventory.hpp"
#include "tasks/task_scheduler.hpp"

using namespace garlic::core;
using namespace garlic::core::tasks;
using garlic::core::tests::FakeClock;
using namespace std::chrono_literals;

namespace
{
    struct Fixture
    {
        FakeClock                            clock;
        common::MemoryFileSystem             fs{clock};
        downloader::tests::MemoryInventory   inventory;
        downloader::tests::FakeHttpTransport transport;
        logger::Logger                       logger{fs, clock, loggerConfig()};
        configuration::Configuration         config{fs, clock, "config/garlic-player.ini"};
        std::vector<TaskAction>              actions;
        downloader::DownloadQueue queue{transport, fs, inventory, clock, downloaderConfig(), 2,
                                        [this](const downloader::DownloadResult &r) { (void)tasks.downloaded(r); }};
        TaskScheduler tasks{queue, fs, logger, config, "cache", [this](TaskAction a) { actions.push_back(std::move(a)); }};

        Fixture()
        {
            REQUIRE(config.load().has_value());
            config.setVersion("1.0");
        }

        static downloader::DownloaderConfig downloaderConfig()
        {
            downloader::DownloaderConfig c;
            c.cache_dir = "cache";
            return c;
        }

        static logger::LoggerConfig loggerConfig()
        {
            logger::LoggerConfig c;
            c.log_dir = "logs";
            return c;
        }

        void serve(const std::string &url, std::string body, const std::string &etag = "v1",
                   const std::string &type = "application/xml")
        {
            downloader::tests::FakeHttpTransport::Response r;
            r.headers.add("Content-Type", type);
            r.headers.add("ETag", "\"" + etag + "\"");
            r.body       = std::move(body);
            r.chunk_size = 1 << 20;
            transport.respond(downloader::HttpMethod::Head, url, r);
            transport.respond(downloader::HttpMethod::Get, url, r);
        }

        void serveList(std::string_view tasks_xml, const std::string &etag = "v1")
        {
            serve("http://hub/tasks.xml", "<schedule><timedTaskList>" + std::string(tasks_xml) + "</timedTaskList></schedule>",
                  etag);
        }

        /// A new index with the subscription TaskSchedule.
        void newIndex()
        {
            tasks.subscribe({{"SystemReport", "http://hub/system", "put", 60, 0}, {"TaskSchedule", "http://hub/tasks.xml", "get", 0, 0}});
            for (int i = 0; i < 100 && !queue.isIdle(); ++i)
            {
                transport.poll();
                queue.poll();
            }
        }

        std::string taskLog() const { return fs.content("logs/task_execution_log.xml").value_or(""); }
        std::string events() const { return fs.content("logs/event_log.xml").value_or(""); }
    };

    const std::string settings = R"(<configuration><prop name="info.playerName" value="Lobby"/>
        <prop name="content.serverUrl" value="http://hub/new.smil"/><prop name="brightness" value="80"/>
        <prop name="volume" value="40"/><prop name="schedule.reboot.days" value="Mo"/></configuration>)";
}

TEST_CASE("no TaskSchedule subscription: nothing is loaded")
{
    Fixture f;
    f.tasks.subscribe({{"SystemReport", "http://hub/system", "put", 60, 0}});
    CHECK(f.transport.requests.empty());
}

TEST_CASE("updateSettings: configuration.xml applied with the ini keys of garlic-lib, once")
{
    Fixture f;
    f.serve("http://hub/config.xml", settings);
    f.serveList(R"(<updateSettings id="s1"><sourceURI>http://hub/config.xml</sourceURI></updateSettings>)");
    f.newIndex();

    CHECK(f.config.playerName() == "Lobby");
    CHECK(f.config.indexUri() == "http://hub/new.smil");
    CHECK(f.config.value("display/brightness") == "80");
    CHECK(f.config.value("audio/volume") == "40");
    CHECK(f.config.rebootDays() == "Mo");
    REQUIRE(f.actions.size() == 1);
    CHECK(std::holds_alternative<ApplyConfiguration>(f.actions[0]));
    CHECK(f.taskLog().find("<task id=\"s1\">") != std::string::npos);
    CHECK(f.taskLog().find("<state>COMPLETED</state>") != std::string::npos);
    CHECK(f.config.value("update_settings_task_id") == "s1");

    f.newIndex(); // the same list again: done
    CHECK(f.actions.size() == 1);
}

TEST_CASE("checksum: right is applied, wrong fails and is tried again with the next index")
{
    Fixture f;
    std::string right = common::Md5::hex(settings);
    f.serve("http://hub/config.xml", settings);
    f.serveList(R"(<updateSettings id="s1"><sourceURI>http://hub/config.xml</sourceURI>
                   <contentChecksum>0000</contentChecksum><checksumMethod>MD5</checksumMethod></updateSettings>)");
    f.newIndex();
    CHECK(f.actions.empty());
    CHECK(f.config.playerName() != "Lobby");
    CHECK(f.taskLog().find("<state>FAILED</state>") != std::string::npos);
    CHECK(f.events().find("CHECKSUM_ERROR") != std::string::npos);

    f.serveList(R"(<updateSettings id="s1"><sourceURI>http://hub/config.xml</sourceURI>
                   <contentChecksum>)" + right + R"(</contentChecksum><checksumMethod>md5</checksumMethod></updateSettings>)",
                "v2");
    f.newIndex();
    CHECK(f.config.playerName() == "Lobby");
    CHECK(f.actions.size() == 1);
}

TEST_CASE("checksum with an unknown method cannot be checked: fails")
{
    Fixture f;
    f.serve("http://hub/config.xml", settings);
    f.serveList(R"(<updateSettings id="s1"><sourceURI>http://hub/config.xml</sourceURI>
                   <contentChecksum>abc</contentChecksum><checksumMethod>SHA-1</checksumMethod></updateSettings>)");
    f.newIndex();
    CHECK(f.actions.empty());
    CHECK(f.events().find("CHECKSUM_UNKNOWN_METHOD") != std::string::npos);
}

TEST_CASE("firmwareUpdate: any content type, InstallSoftware, PENDING; the next start checks the version")
{
    Fixture f;
    f.serve("http://hub/fw", "binary", "v1", "application/octet-stream");
    f.serveList(R"(<firmwareUpdate id="f1"><sourceURI>http://hub/fw</sourceURI><version>2.0</version>
                   <contentChecksum>)" + common::Md5::hex("binary") + R"(</contentChecksum></firmwareUpdate>)");
    f.newIndex();

    REQUIRE(f.actions.size() == 1);
    auto install = std::get<InstallSoftware>(f.actions[0]);
    CHECK(install.task_id == "f1");
    CHECK(install.path.starts_with("cache/garlic-player."));
    CHECK(f.fs.content(install.path) == "binary");
    CHECK(f.taskLog().find("<state>PENDING</state>") != std::string::npos);

    // restart with the new version
    f.config.setVersion("2.0");
    f.tasks.checkPending();
    CHECK(f.taskLog().find("<state>COMPLETED</state>") != std::string::npos);
    CHECK(f.config.value("tasks/pending_id").empty());
}

TEST_CASE("firmwareUpdate: still the old version after the restart is FAILED")
{
    Fixture f;
    f.serve("http://hub/fw", "binary", "v1", "application/octet-stream");
    f.serveList(R"(<firmwareUpdate id="f1"><sourceURI>http://hub/fw</sourceURI><version>2.0</version></firmwareUpdate>)");
    f.newIndex();
    f.tasks.checkPending(); // version is still 1.0
    CHECK(f.taskLog().find("<state>FAILED</state>") != std::string::npos);
}

TEST_CASE("shutdownPlayer: Reboot once, COMPLETED after the restart")
{
    Fixture f;
    f.serveList(R"(<shutdownPlayer id="r1"/>)");
    f.newIndex();
    REQUIRE(f.actions.size() == 1);
    CHECK(std::get<Reboot>(f.actions[0]).task_id == "r1");
    CHECK(f.config.value("shutdown_player_task_id") == "r1"); // done before: no endless restarts

    f.tasks.checkPending();
    CHECK(f.taskLog().find("<state>COMPLETED</state>") != std::string::npos);
    f.newIndex();
    CHECK(f.actions.size() == 1);
}

TEST_CASE("applyCommand: which caches, with its own task id")
{
    Fixture f;
    f.serveList(R"(<applyCommand id="a1"><command>clear_all_caches</command></applyCommand>
                   <applyCommand id="a2"><command>clear_cache</command></applyCommand>
                   <applyCommand id="a3"><command>clear_webcache</command></applyCommand>)");
    f.newIndex();
    REQUIRE(f.actions.size() == 3);
    auto all = std::get<ClearCaches>(f.actions[0]);
    CHECK(all.task_id == "a1"); // garlic-lib reported the id of shutdownPlayer
    CHECK(all.player);
    CHECK(all.web);
    CHECK(std::get<ClearCaches>(f.actions[1]).player);
    CHECK_FALSE(std::get<ClearCaches>(f.actions[1]).web);
    CHECK(std::get<ClearCaches>(f.actions[2]).web);
}

TEST_CASE("two tasks of the same kind run once each (garlic-lib ran the first again with every index)")
{
    Fixture f;
    f.serveList(R"(<shutdownPlayer id="r1"/><shutdownPlayer id="r2"/>)");
    f.newIndex();
    CHECK(f.actions.size() == 2);
    f.newIndex();
    CHECK(f.actions.size() == 2);
}

TEST_CASE("the done id under the old key of garlic-lib is honoured")
{
    Fixture f;
    f.config.setValue("shutdown_player_task_id", "r1");
    f.serveList(R"(<shutdownPlayer id="r1"/>)");
    f.newIndex();
    CHECK(f.actions.empty());
}

TEST_CASE("a broken task list is an event, nothing runs")
{
    Fixture f;
    f.serve("http://hub/tasks.xml", "<schedule><timedTaskList>");
    f.newIndex();
    CHECK(f.actions.empty());
    CHECK(f.events().find("TASK_SCHEDULER_XML_ERROR") != std::string::npos);
}

TEST_CASE("a local configuration.xml from the launcher")
{
    Fixture f;
    auto file = f.fs.openForWrite("usb/configuration.xml", common::WriteMode::Truncate);
    REQUIRE(file.has_value());
    REQUIRE((*file)->write(std::as_bytes(std::span(settings.data(), settings.size()))).has_value());
    REQUIRE((*file)->close().has_value());

    CHECK(f.tasks.applyConfigurationFile("usb/configuration.xml"));
    CHECK(f.config.playerName() == "Lobby");
    CHECK_FALSE(f.tasks.applyConfigurationFile("usb/missing.xml"));
}
