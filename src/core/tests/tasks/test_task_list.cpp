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

#include "tasks/task_list.hpp"

using namespace garlic::core::tasks;

TEST_CASE("task list: the four kinds with their fields, unknown ones ignored")
{
    auto tasks = parseTaskList(R"(<?xml version="1.0"?>
        <schedule xmlns="http://schemas.adfotain.org/adapi-1.1"><timedTaskList>
            <updateSettings id="s1"><sourceURI> http://hub/config.xml </sourceURI><contentType>application/xml</contentType>
                <contentChecksum>ABC</contentChecksum><checksumMethod>MD5</checksumMethod></updateSettings>
            <firmwareUpdate id="f1"><sourceURI>http://hub/fw</sourceURI><version>2.0</version></firmwareUpdate>
            <somethingNew id="x"/>
            <shutdownPlayer id="r1"/>
            <applyCommand id="c1"><command>clear_cache</command></applyCommand>
        </timedTaskList></schedule>)");
    REQUIRE(tasks.has_value());
    REQUIRE(tasks->size() == 4);

    CHECK((*tasks)[0].kind == Task::Kind::UpdateSettings);
    CHECK((*tasks)[0].id == "s1");
    CHECK((*tasks)[0].source_uri == "http://hub/config.xml");
    CHECK((*tasks)[0].content_checksum == "ABC");
    CHECK((*tasks)[0].checksum_method == "MD5");
    CHECK((*tasks)[1].kind == Task::Kind::FirmwareUpdate);
    CHECK((*tasks)[1].version == "2.0");
    CHECK((*tasks)[2].kind == Task::Kind::ShutdownPlayer);
    CHECK((*tasks)[3].kind == Task::Kind::ApplyCommand);
    CHECK((*tasks)[3].command == "clear_cache");
}

TEST_CASE("task list: without timedTaskList empty, broken XML with line")
{
    auto empty = parseTaskList("<schedule/>");
    REQUIRE(empty.has_value());
    CHECK(empty->empty());

    auto broken = parseTaskList("<schedule>\n<timedTaskList>\n</schedule>");
    REQUIRE_FALSE(broken.has_value());
    CHECK(broken.error().line == 3);
}

TEST_CASE("settings: every prop with a value")
{
    auto settings = parseSettings(R"(<configuration><userPref>
        <prop name="info.playerName" value="Lobby"/><prop name="volume" value=""/>
        <prop name="content.serverUrl" value="http://hub/index.smil"/></userPref></configuration>)");
    REQUIRE(settings.has_value());
    REQUIRE(settings->size() == 2);
    CHECK((*settings)[0] == std::pair<std::string, std::string>{"info.playerName", "Lobby"});
    CHECK((*settings)[1].second == "http://hub/index.smil");
}
