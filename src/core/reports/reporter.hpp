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
#ifndef GARLIC_CORE_REPORTS_REPORTER_HPP
#define GARLIC_CORE_REPORTS_REPORTER_HPP

#include "common/clock.hpp"
#include "common/file_system.hpp"
#include "configuration/configuration.hpp"
#include "database/inventory_table.hpp"
#include "downloader/http_transport.hpp"
#include "logger/logger.hpp"
#include "reports/system_info.hpp"
#include "smil/document.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace garlic::core::reports
{
    /**
     * Sends the reports to the hub as the subscriptions of the index say, garlic-lib: Reporting::*Manager.
     * Every upload is an HTTP PUT with Content-Type application/xml.
     *
     *   PlaylogCollection   play logs, oldest first, one file per upload to <action>/playlog-<id>.xml, deleted after
     *   EventlogCollection  the same with event logs to <action>/event-<id>.xml
     *   InventoryReport     the files in the cache to <action>
     *   SystemReport        the device to <action>
     *
     * Sent at once when a subscription is new or changed, then every refreshInterval seconds (0: only once). After a
     * failed upload the next try comes after retryInterval seconds (garlic-lib ignored it and waited for the next
     * refresh). A log file is renamed to "<log>.sending" while it is uploaded, so a rotation of the logger cannot move
     * it; it is deleted only after a successful upload and sent first at the next try.
     */
    class Reporter
    {
        public:
            using SystemInfoSource = std::function<SystemInfo()>;

            Reporter(downloader::HttpTransport &transport, common::FileSystem &fs, const common::Clock &clock,
                     logger::Logger &logger, configuration::Configuration &config,
                     const database::InventoryTable &inventory, std::string cache_dir, SystemInfoSource system_info);
            ~Reporter();

            Reporter(const Reporter &)            = delete;
            Reporter &operator=(const Reporter &) = delete;

            /// The subscriptions of a new index. Unchanged ones keep their schedule.
            void subscribe(const std::vector<smil::Subscription> &subscriptions);

            /// Starts the uploads that are due. The host polls the transport too.
            void poll();

            std::optional<std::chrono::steady_clock::time_point> nextDeadline() const;

        private:
            enum class Kind
            {
                PlayLog,
                EventLog,
                Inventory,
                System
            };

            struct Job;

            downloader::HttpTransport      &transport;
            common::FileSystem             &fs;
            const common::Clock            &clock;
            logger::Logger                 &logger;
            configuration::Configuration   &config;
            const database::InventoryTable &inventory;
            std::string                     cache_dir;
            SystemInfoSource                system_info;
            std::vector<std::unique_ptr<Job>> jobs;

            void        send(Job &job);
            void        finished(Job &job, bool ok);
            void        schedule(Job &job, int seconds);
            std::string nextLogFile(Kind kind);
            std::string body(Job &job);
            std::string url(const Job &job) const;
    };
}

#endif // GARLIC_CORE_REPORTS_REPORTER_HPP
