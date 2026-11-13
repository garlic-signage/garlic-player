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
#include "failing_file_system.hpp"
#include "fake_clock.hpp"
#include "fake_http_transport.hpp"
#include "memory_inventory.hpp"
#include "tasks/task_scheduler.hpp"

#include <algorithm>
#include <cctype>
#include <functional>

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
        garlic::core::tests::FailingFileSystem failing{fs}; // for the scheduler: nothing fails unless a test says so
        downloader::tests::MemoryInventory   inventory;
        downloader::tests::FakeHttpTransport transport;
        logger::Logger                       logger{fs, clock, loggerConfig()};
        configuration::Configuration         config{fs, clock, "config/garlic-player.ini"};
        std::vector<TaskAction>              actions;
        std::function<void(const TaskAction &)> spy; // looks at the state at the moment of the action
        downloader::DownloadQueue queue{transport, fs, inventory, clock, downloaderConfig(), 2,
                                        [this](const downloader::DownloadResult &r) { (void)tasks.downloaded(r); }};
        TaskScheduler tasks{queue, failing, logger, config, "cache", [this](TaskAction a)
                                {
                                    if (spy)
                                        spy(a);
                                    actions.push_back(std::move(a));
                                }};

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

        void write(const std::string &path, std::string_view text)
        {
            auto file = fs.openForWrite(path, common::WriteMode::Truncate);
            REQUIRE(file.has_value());
            REQUIRE((*file)->write(std::as_bytes(std::span(text.data(), text.size()))).has_value());
            REQUIRE((*file)->close().has_value());
        }

        std::string taskLog() const { return fs.content("logs/task_execution_log.xml").value_or(""); }
        std::string events() const { return fs.content("logs/event_log.xml").value_or(""); }

        /// The first event with this name, empty if there is none.
        std::string event(std::string_view name) const
        {
            std::string all = events();
            auto        at  = all.find("<eventName>" + std::string(name) + "</eventName>");
            if (at == std::string::npos)
                return {};
            auto begin = all.rfind("<event>", at);
            return all.substr(begin, all.find("</event>", at) - begin);
        }
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
    CHECK(f.taskLog().find("<state>completed</state>") != std::string::npos);
    CHECK(f.config.value("update_settings_task_id") == "s1");

    // the fixed names in the cache, the events with size and time of the files
    CHECK(f.fs.exists("cache/task_scheduler.xml"));
    CHECK(f.fs.exists("cache/configuration.xml"));
    std::string list_loaded = f.event("TASK_SCHEDULER_LOADED");
    CHECK(list_loaded.find(R"(<meta name="resourceURI" content="cache/task_scheduler.xml"/>)") != std::string::npos);
    CHECK(list_loaded.find("lastModifiedTime") != std::string::npos);
    std::string config_loaded = f.event("CONFIGURATION_LOADED");
    CHECK(config_loaded.find(R"(<meta name="contentLength" content=")" + std::to_string(settings.size()) + "\"/>") !=
          std::string::npos);
    CHECK(config_loaded.find("lastModifiedTime") != std::string::npos);

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
    CHECK(f.taskLog().find("<state>aborted</state>") != std::string::npos);
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

TEST_CASE("firmwareUpdate: any content type, InstallSoftware, pending; the next start checks the version")
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
    CHECK(f.taskLog().find("<state>pending</state>") != std::string::npos);
    if (configuration::Configuration::os() == "linux")
        CHECK(install.path == "cache/garlic-player.tar.xz"); // the name the launcher expects

    f.newIndex(); // the same list again: not installed twice
    CHECK(f.actions.size() == 1);

    // restart with the new version
    f.config.setVersion("2.0");
    f.tasks.checkPending();
    CHECK(f.taskLog().find("<state>completed</state>") != std::string::npos);
    CHECK(f.config.value("tasks/pending_id").empty());
}

TEST_CASE("firmwareUpdate: still the old version after the restart is aborted")
{
    Fixture f;
    f.serve("http://hub/fw", "binary", "v1", "application/octet-stream");
    f.serveList(R"(<firmwareUpdate id="f1"><sourceURI>http://hub/fw</sourceURI><version>2.0</version></firmwareUpdate>)");
    f.newIndex();
    f.tasks.checkPending(); // version is still 1.0
    CHECK(f.taskLog().find("<state>aborted</state>") != std::string::npos);
}

TEST_CASE("shutdownPlayer: Reboot once, completed after the restart")
{
    Fixture f;
    f.spy = [&f](const TaskAction &) // the host may restart at once: everything is stored before
    {
        CHECK(f.config.value("shutdown_player_task_id") == "r1");
        CHECK(f.config.value("tasks/pending_id") == "r1");
    };
    f.serveList(R"(<shutdownPlayer id="r1"/>)");
    f.newIndex();
    REQUIRE(f.actions.size() == 1);
    CHECK(std::get<Reboot>(f.actions[0]).task_id == "r1");
    CHECK(f.config.value("shutdown_player_task_id") == "r1"); // done before: no endless restarts

    f.tasks.checkPending();
    CHECK(f.taskLog().find("<state>completed</state>") != std::string::npos);
    f.newIndex();
    CHECK(f.actions.size() == 1);
}

TEST_CASE("applyCommand: which caches, with its own task id")
{
    Fixture f;
    f.spy = [&f](const TaskAction &action) // done and pending before the restart
    {
        std::string id = std::get<ClearCaches>(action).task_id;
        CHECK(f.config.value("apply_command_task_id") == id);
        CHECK(f.config.value("tasks/pending_id") == id);
    };
    f.serveList(R"(<applyCommand id="a1"><command>clear_all_caches</command></applyCommand>
                   <applyCommand id="a2"><command>clear_cache</command></applyCommand>
                   <applyCommand id="a3"><command>clear_webcache</command></applyCommand>
                   <applyCommand id="a4"><command>clear_playercache</command></applyCommand>)");
    f.newIndex();
    REQUIRE(f.actions.size() == 4);
    auto all = std::get<ClearCaches>(f.actions[0]);
    CHECK(all.task_id == "a1"); // garlic-lib reported the id of shutdownPlayer
    CHECK(all.player);
    CHECK(all.web);
    CHECK(std::get<ClearCaches>(f.actions[1]).player);
    CHECK_FALSE(std::get<ClearCaches>(f.actions[1]).web);
    CHECK(std::get<ClearCaches>(f.actions[2]).web);
    CHECK_FALSE(std::get<ClearCaches>(f.actions[2]).player);
    CHECK(std::get<ClearCaches>(f.actions[3]).player);
    CHECK_FALSE(std::get<ClearCaches>(f.actions[3]).web);
    CHECK(f.taskLog().find("<state>pending</state>") != std::string::npos);

    f.newIndex(); // done
    CHECK(f.actions.size() == 4);
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
    std::string error = f.event("TASK_SCHEDULER_XML_ERROR");
    CHECK(error.find(R"(<meta name="errorLine" content="1"/>)") != std::string::npos);
    CHECK(error.find(R"(<meta name="errorMessage")") != std::string::npos);
}

TEST_CASE("a local configuration.xml from the launcher")
{
    Fixture f;
    f.write("usb/configuration.xml", settings);

    CHECK(f.tasks.applyConfigurationFile("usb/configuration.xml"));
    CHECK(f.config.playerName() == "Lobby");
    CHECK_FALSE(f.tasks.applyConfigurationFile("usb/missing.xml"));
    CHECK(f.event("CONFIGURATION_OPEN_ERROR").find(R"(content="usb/missing.xml")") != std::string::npos);
}

TEST_CASE("settings: all props of garlic-lib, also under their other names")
{
    Fixture f;
    f.write("a.xml", R"(<configuration>
        <prop name="display.brightness" value="70"/><prop name="hardware.videoOut.0.rotation" value="90"/>
        <prop name="audio.soundlevel" value="30"/><prop name="time.tzDescription" value="Europe/Berlin"/>
        <prop name="time.tzCode" value="CET"/><prop name="time.autoUpdate.protocol" value="NTP"/>
        <prop name="time.autoUpdate.server" value="pool.ntp.org"/><prop name="task.scheduledReboot.days" value="Su"/>
        <prop name="task.scheduledReboot.time" value="03:00"/><prop name="standby_mode" value="partly"/>
        <prop name="unknown" value="x"/></configuration>)");
    REQUIRE(f.tasks.applyConfigurationFile("a.xml"));
    CHECK(f.config.value("display/brightness") == "70");
    CHECK(f.config.value("display/rotation") == "90");
    CHECK(f.config.value("audio/volume") == "30");
    CHECK(f.config.value("time/tzDescription") == "Europe/Berlin");
    CHECK(f.config.value("time/tzCode") == "CET");
    CHECK(f.config.value("time/protocol") == "NTP");
    CHECK(f.config.value("time/server") == "pool.ntp.org");
    CHECK(f.config.rebootDays() == "Su");
    CHECK(f.config.rebootTime() == "03:00");
    CHECK(f.config.standbyMode() == "partly");
    CHECK(f.config.value("unknown").empty());

    f.write("b.xml", R"(<configuration><prop name="hardware.audioOut.0.masterSoundLevel" value="20"/>
        <prop name="schedule.reboot.time" value="04:00"/></configuration>)");
    REQUIRE(f.tasks.applyConfigurationFile("b.xml"));
    CHECK(f.config.value("audio/volume") == "20");
    CHECK(f.config.rebootTime() == "04:00");
}

TEST_CASE("updateSettings with a broken configuration.xml: aborted, nothing applied, tried again")
{
    Fixture f;
    f.serve("http://hub/config.xml", "<configuration><prop name=\"info.playerName\" value=\"Lobby\"/>");
    f.serveList(R"(<updateSettings id="s1"><sourceURI>http://hub/config.xml</sourceURI></updateSettings>)");
    f.newIndex();
    CHECK(f.actions.empty());
    CHECK(f.config.playerName() != "Lobby");
    CHECK(f.taskLog().find("<state>aborted</state>") != std::string::npos);
    CHECK(f.config.value("update_settings_task_id").empty());
    CHECK(f.event("CONFIGURATION_XML_ERROR").find("errorLine") != std::string::npos);

    f.write("broken.xml", "<configuration>");
    CHECK_FALSE(f.tasks.applyConfigurationFile("broken.xml"));
}

TEST_CASE("checksum and method in upper case")
{
    Fixture f;
    std::string config = R"(<configuration><prop name="info.playerName" value="Lobby"/></configuration>)";
    std::string right  = common::Md5::hex(config);
    REQUIRE(right.find('a') != std::string::npos);
    std::ranges::transform(right, right.begin(), [](char c) { return static_cast<char>(std::toupper(c)); });
    f.serve("http://hub/config.xml", config);
    f.serveList(R"(<updateSettings id="s1"><sourceURI>http://hub/config.xml</sourceURI>
                   <contentChecksum>)" + right + R"(</contentChecksum><checksumMethod>MD5</checksumMethod></updateSettings>)");
    f.newIndex();
    CHECK(f.config.playerName() == "Lobby");
}

TEST_CASE("the version of a restart task is not checked, only the one of firmwareUpdate")
{
    Fixture f;
    f.serveList(R"(<shutdownPlayer id="r1"><version>9.0</version></shutdownPlayer>)");
    f.newIndex();
    f.tasks.checkPending();
    CHECK(f.taskLog().find("<state>completed</state>") != std::string::npos);
}

TEST_CASE("start without a pending task: no task state")
{
    Fixture f;
    f.tasks.checkPending();
    CHECK(f.taskLog().empty());
}

TEST_CASE("a task without id is not run, also when one of its kind with id was done before")
{
    Fixture f;
    f.serveList(R"(<shutdownPlayer id="r1"/>)");
    f.newIndex();
    REQUIRE(f.actions.size() == 1);

    f.serveList(R"(<shutdownPlayer/><applyCommand><command>clear_cache</command></applyCommand>)", "v2");
    f.newIndex();
    CHECK(f.actions.size() == 1);
}

TEST_CASE("the last 20 done ids per kind are kept")
{
    Fixture f;
    std::string list;
    for (int i = 1; i <= 21; ++i)
        list += R"(<shutdownPlayer id="r)" + std::to_string(i) + R"("/>)";
    f.serveList(list);
    f.newIndex();
    CHECK(f.actions.size() == 21);

    std::string expected;
    for (int i = 2; i <= 21; ++i)
        expected += (i == 2 ? "r" : ",r") + std::to_string(i);
    CHECK(f.config.value("shutdown_player_task_ids") == expected);
}

TEST_CASE("TaskSchedule without URL, or gone with the next index: the list is not loaded or used")
{
    Fixture f;
    f.tasks.subscribe({{"TaskSchedule", "", "get", 0, 0}});
    CHECK(f.transport.requests.empty());

    f.serveList(R"(<shutdownPlayer id="r1"/>)");
    f.tasks.subscribe({{"TaskSchedule", "http://hub/tasks.xml", "get", 0, 0}});
    f.tasks.subscribe({{"SystemReport", "http://hub/system", "put", 60, 0}}); // before the list arrived
    for (int i = 0; i < 100 && !f.queue.isIdle(); ++i)
    {
        f.transport.poll();
        f.queue.poll();
    }
    CHECK(f.actions.empty());
}

TEST_CASE("a task list that cannot be downloaded: nothing runs, no open error")
{
    Fixture f;
    f.newIndex(); // tasks.xml is not served
    CHECK(f.actions.empty());
    CHECK(f.event("TASK_SCHEDULER_OPEN_ERROR").empty());
}

// ------------------------------------------------------------------ errors of the file system

TEST_CASE("a task list that cannot be read: TASK_SCHEDULER_OPEN_ERROR, nothing runs")
{
    Fixture f;
    f.failing.fail_read = true;
    f.failing.only      = "task_scheduler.xml";
    f.serveList(R"(<shutdownPlayer id="r1"/>)");
    f.newIndex();
    CHECK(f.actions.empty());
    CHECK(f.event("TASK_SCHEDULER_OPEN_ERROR").contains(R"(content="cache/task_scheduler.xml")"));
}

TEST_CASE("a new task list that cannot replace the old one waits, the next index takes it")
{
    Fixture f;
    f.serveList("");
    f.newIndex();

    f.failing.fail_rename = true;
    f.failing.only        = "task_scheduler.xml.ready";
    f.serveList(R"(<shutdownPlayer id="r1"/>)", "v2");
    f.newIndex();
    CHECK(f.actions.empty());

    f.failing.fail_rename = false;
    f.newIndex();
    CHECK(f.actions.size() == 1);
}

TEST_CASE("a configuration.xml that cannot be read: aborted, tried again with the next index")
{
    Fixture f;
    f.failing.fail_read = true;
    f.failing.only      = "configuration.xml";
    f.serve("http://hub/config.xml", settings);
    f.serveList(R"(<updateSettings id="s1"><sourceURI>http://hub/config.xml</sourceURI></updateSettings>)");
    f.newIndex();
    CHECK(f.actions.empty());
    CHECK(f.taskLog().contains("<state>aborted</state>"));
    CHECK(f.config.value("update_settings_task_id").empty());

    f.failing.fail_read = false;
    f.newIndex();
    CHECK(f.config.playerName() == "Lobby");
}
