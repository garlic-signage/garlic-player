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
#include "logger/logger.hpp"

#include "common/time_format.hpp"

namespace garlic::core::logger
{
    namespace
    {
        std::string escapeXml(std::string_view text)
        {
            std::string out;
            out.reserve(text.size());
            for (char c : text)
            {
                switch (c)
                {
                    case '&': out += "&amp;"; break;
                    case '<': out += "&lt;"; break;
                    case '>': out += "&gt;"; break;
                    case '"': out += "&quot;"; break;
                    case '\'': out += "&apos;"; break;
                    default: out.push_back(c);
                }
            }
            return out;
        }

        std::string_view fileName(std::string_view path)
        {
            auto slash = path.find_last_of("/\\");
            return slash == std::string_view::npos ? path : path.substr(slash + 1);
        }

        std::string_view baseName(LogFile file)
        {
            switch (file)
            {
                case LogFile::Debug: return "debug.log";
                case LogFile::Event: return "event_log.xml";
                case LogFile::Play: return "play_log.xml";
                case LogFile::TaskExecution: return "task_execution_log.xml";
            }
            return "debug.log";
        }
    }

    std::string_view toString(Severity severity)
    {
        switch (severity)
        {
            case Severity::Debug: return "debug";
            case Severity::Informational: return "informational";
            case Severity::Warning: return "warning";
            case Severity::Error: return "error";
            case Severity::Critical: return "critical";
        }
        return "UNKNOWN";
    }

    Logger::Logger(common::FileSystem &f, const common::Clock &c, LoggerConfig cfg)
        : fs(f), clock(c), config(std::move(cfg))
    {
        (void)fs.createDirectories(config.log_dir);
    }

    void Logger::log(Severity severity, std::string_view category, std::string_view message,
                     std::source_location where)
    {
        if (severity < config.min_debug_severity)
            return;

        std::string line = common::logTimestamp(clock.now()) + " " + std::string(toString(severity)) + " " +
                           std::string(category) + " " + std::string(fileName(where.file_name())) + "(" +
                           std::to_string(where.line()) + ") " + std::string(message);
        append(LogFile::Debug, line);
        if (config.mirror)
            config.mirror(severity, category, line);
    }

    void Logger::event(Severity severity, std::string_view source, std::string_view name, const Metadata &metadata)
    {
        std::string line = "<event><eventType>" + std::string(toString(severity)) + "</eventType><eventTime>" +
                           common::isoLocalTime(clock.now()) + "</eventTime><eventSource>" + escapeXml(source) +
                           "</eventSource><eventName>" + escapeXml(name) + "</eventName><metadata>";
        for (const auto &[key, value] : metadata)
            line += "<meta name=\"" + escapeXml(key) + "\" content=\"" + escapeXml(value) + "\"/>";
        line += "</metadata></event>";

        append(LogFile::Event, line);
        if (config.mirror)
            config.mirror(severity, source, line);
    }

    void Logger::played(std::string_view content_id, std::chrono::system_clock::time_point start)
    {
        append(LogFile::Play, "<contentPlayed><contentId>" + escapeXml(content_id) + "</contentId><startTime>" +
                                  common::isoLocalTime(start) + "</startTime><endTime>" +
                                  common::isoLocalTime(clock.now()) + "</endTime></contentPlayed>");
    }

    void Logger::taskState(std::string_view task_id, std::string_view state)
    {
        append(LogFile::TaskExecution, "<taskState><task id=\"" + escapeXml(task_id) + "\"><lastUpdateTime>" +
                                           common::isoLocalTime(clock.now()) + "</lastUpdateTime><state>" +
                                           escapeXml(state) + "</state></task></taskState>");
    }

    std::string Logger::path(LogFile file) const
    {
        return config.log_dir + "/" + std::string(baseName(file));
    }

    void Logger::rotate(LogFile file)
    {
        // name.29 → name.30 … name → name.1; the oldest is deleted
        std::string base = path(file);
        (void)fs.remove(base + "." + std::to_string(config.max_files));
        for (int n = config.max_files - 1; n >= 1; --n)
        {
            std::string from = base + "." + std::to_string(n);
            if (fs.exists(from))
                (void)fs.rename(from, base + "." + std::to_string(n + 1));
        }
        if (fs.exists(base))
            (void)fs.rename(base, base + ".1");
    }

    void Logger::append(LogFile file, std::string_view line)
    {
        std::string target = path(file);
        auto        out    = fs.openForWrite(target, common::WriteMode::Append);
        if (!out)
            return;

        std::string text = std::string(line) + "\n";
        (void)(*out)->write(std::as_bytes(std::span(text.data(), text.size())));
        (void)(*out)->close();

        std::uint64_t limit = file == LogFile::Debug ? config.debug_max_size : config.max_size;
        if (fs.fileSize(target).value_or(0) >= limit)
            rotate(file);
    }
}
