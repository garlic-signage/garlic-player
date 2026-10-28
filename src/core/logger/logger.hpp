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
#ifndef GARLIC_CORE_LOGGER_LOGGER_HPP
#define GARLIC_CORE_LOGGER_LOGGER_HPP

#include "common/clock.hpp"
#include "common/file_system.hpp"

#include <cstdint>
#include <functional>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace garlic::core::logger
{
    /// Severities with the names garlic-lib wrote into the logs.
    enum class Severity
    {
        Debug,         // "debug"
        Informational, // "informational"
        Warning,       // "warning"
        Error,         // "error"
        Critical       // "critical"
    };

    std::string_view toString(Severity severity);

    /// The log files in the log directory, same names as garlic-lib.
    enum class LogFile
    {
        Debug,        // debug.log: text lines for developers
        Event,        // event_log.xml: events, sent to the server
        Play,         // play_log.xml: what was played, sent to the server
        TaskExecution // task_execution_log.xml: state of tasks
    };

    using Metadata = std::vector<std::pair<std::string, std::string>>;

    struct LoggerConfig
    {
        std::string   log_dir;                    // given by the frontend
        std::uint64_t max_size       = 1048576;   // 1 MiB, then the file is rotated
        std::uint64_t debug_max_size = 10485760;  // 10 MiB for debug.log
        int           max_files      = 30;        // name.1 … name.30, older ones are deleted
        Severity      min_debug_severity = Severity::Debug; // e.g. Warning on ESP32 to spare the flash

        /// Optional: every entry also goes here, e.g. to the console or logcat of the frontend.
        std::function<void(Severity, std::string_view category, std::string_view text)> mirror;
    };

    /**
     * Writes the logs of the player. Each entry is appended and the file closed again, so nothing is lost on a crash
     * or power cut. Write errors are ignored: logging never stops the player.
     */
    class Logger
    {
        public:
            Logger(common::FileSystem &fs, const common::Clock &clock, LoggerConfig config);

            // --- debug.log
            void log(Severity severity, std::string_view category, std::string_view message,
                     std::source_location where = std::source_location::current());

            void debug(std::string_view category, std::string_view message,
                       std::source_location where = std::source_location::current())
            {
                log(Severity::Debug, category, message, where);
            }
            void info(std::string_view category, std::string_view message,
                      std::source_location where = std::source_location::current())
            {
                log(Severity::Informational, category, message, where);
            }
            void warning(std::string_view category, std::string_view message,
                         std::source_location where = std::source_location::current())
            {
                log(Severity::Warning, category, message, where);
            }
            void error(std::string_view category, std::string_view message,
                       std::source_location where = std::source_location::current())
            {
                log(Severity::Error, category, message, where);
            }

            // --- event_log.xml, e.g. event(Severity::Warning, "ContentManager", "MEDIA_UNSUPPORTED", {{"src", url}})
            void event(Severity severity, std::string_view source, std::string_view name, const Metadata &metadata = {});

            // --- play_log.xml: content played from start until now
            void played(std::string_view content_id, std::chrono::system_clock::time_point start);

            // --- task_execution_log.xml
            void taskState(std::string_view task_id, std::string_view state);

            /// Rotates a log, e.g. before it is sent to the server, so the sent entries are not sent again.
            void rotate(LogFile file);

            std::string path(LogFile file) const;

        private:
            void append(LogFile file, std::string_view line);

            common::FileSystem  &fs;
            const common::Clock &clock;
            LoggerConfig         config;
    };
}

#endif // GARLIC_CORE_LOGGER_LOGGER_HPP
