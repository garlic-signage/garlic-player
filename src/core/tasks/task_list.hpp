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
#ifndef GARLIC_CORE_TASKS_TASK_LIST_HPP
#define GARLIC_CORE_TASKS_TASK_LIST_HPP

#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace garlic::core::tasks
{
    /// One entry of <timedTaskList>, garlic-lib: TaskScheduler::parse*.
    struct Task
    {
        enum class Kind
        {
            UpdateSettings, // <updateSettings>: a configuration.xml to apply
            FirmwareUpdate, // <firmwareUpdate>: new software to install
            ShutdownPlayer, // <shutdownPlayer>: restart the device
            ApplyCommand    // <applyCommand><command>clear_cache</command>
        };
        Kind        kind = Kind::ShutdownPlayer;
        std::string id;
        std::string source_uri;
        std::string version;          // firmware
        std::string content_type;
        std::string content_checksum; // hex
        std::string checksum_method;  // MD5
        std::string command;          // applyCommand
    };

    struct XmlError
    {
        int         line = 0;
        std::string message;
    };

    /// <… ><timedTaskList><updateSettings id="…"><sourceURI>…</sourceURI>…</updateSettings>…</timedTaskList>
    std::expected<std::vector<Task>, XmlError> parseTaskList(std::string_view xml);

    /// configuration.xml: every <prop name="…" value="…"/> with a value, in document order.
    std::expected<std::vector<std::pair<std::string, std::string>>, XmlError> parseSettings(std::string_view xml);
}

#endif // GARLIC_CORE_TASKS_TASK_LIST_HPP
