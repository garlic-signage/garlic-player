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
#ifndef GARLIC_CORE_DOWNLOADER_DOWNLOAD_QUEUE_HPP
#define GARLIC_CORE_DOWNLOADER_DOWNLOAD_QUEUE_HPP

#include "downloader/downloader.hpp"

#include <deque>
#include <functional>
#include <string_view>
#include <memory>
#include <string>
#include <vector>

namespace garlic::core::downloader
{
    /**
     * Distributes download jobs to a fixed number of parallel Downloaders.
     *
     * - Jobs run in the order they were added.
     * - A URL that is already waiting or running is not added a second time.
     * - Every job ends with exactly one call of on_finished, always from inside a poll().
     *
     * The host calls HttpTransport::poll() and DownloadQueue::poll() regularly.
     */
    class DownloadQueue
    {
        public:
            DownloadQueue(HttpTransport &transport, common::FileSystem &fs, Inventory &inventory,
                          const common::Clock &clock, const DownloaderConfig &config, std::size_t max_parallel,
                          Downloader::FinishedCallback on_finished);

            /// Adds a job. file_name: fixed name in the cache (e.g. "index.smil"), empty = derived from the URL.
            void add(std::string url, std::string file_name = {});

            /// Removes all waiting jobs and cancels the running ones, without calling on_finished.
            void clear();

            /// Checks the idle timeouts of the running downloads.
            void poll();

            /// For all downloaders, from their next request on.
            void setUserAgent(const std::string &user_agent);

            std::size_t pendingCount() const noexcept { return pending.size(); }
            std::size_t runningCount() const noexcept;
            bool        isIdle() const noexcept { return pending.empty() && runningCount() == 0; }

            using UrlFilter = std::function<bool(std::string_view url)>;

            /// The waiting and running jobs whose URL the filter takes
            std::size_t count(const UrlFilter &filter) const;

            /// Of the running download that started first among those the filter takes (empty: all); nothing while
            /// none runs
            Downloader::Progress firstProgress(const UrlFilter &filter = {}) const;

            struct Running
            {
                std::string          url;
                Downloader::Progress progress;
            };

            /// The running downloads in the order they started
            std::vector<Running> running() const;

        private:
            struct Job
            {
                std::string url;
                std::string file_name;
            };

            struct Slot
            {
                std::unique_ptr<Downloader> downloader;
                std::string                 url; // of the running job
                std::uint64_t               started = 0; // order of the start, for firstProgress()
            };

            void startPending();
            bool contains(std::string_view url) const;

            std::vector<Slot>            slots;
            std::deque<Job>              pending;
            Downloader::FinishedCallback on_finished;
            std::uint64_t                starts = 0;
    };
}

#endif // GARLIC_CORE_DOWNLOADER_DOWNLOAD_QUEUE_HPP
