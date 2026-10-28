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
#ifndef GARLIC_CORE_DOWNLOADER_DOWNLOADER_HPP
#define GARLIC_CORE_DOWNLOADER_DOWNLOADER_HPP

#include "common/clock.hpp"
#include "common/file_system.hpp"
#include "downloader/http_transport.hpp"
#include "downloader/inventory.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace garlic::core::downloader
{
    enum class DownloadStatus
    {
        Downloaded,   // new file in the cache
        NotModified,  // cached file is up to date
        NotCacheable, // e.g. a web page; the player uses the URL directly
        Failed        // see error; a cached copy is still in local_path if there is one
    };

    struct DownloadResult
    {
        DownloadStatus status = DownloadStatus::Failed;
        std::string    url;

        /// File the player plays. Empty if there is none (NotCacheable, or Failed without cached copy).
        /// If the file existed already, the new download waits next to it as local_path + ".ready".
        std::string local_path;

        std::string error;           // reason for Failed, for logging
        int         http_status = 0; // last HTTP status, 0 if none was received
    };

    struct DownloaderConfig
    {
        std::string          cache_dir; // without trailing '/'
        std::string          user_agent;
        int                  max_redirects = 5;
        std::chrono::seconds idle_timeout{60}; // abort if no data arrives for this long

        /// Called before a download with the needed bytes. Returns false if the space cannot be made available.
        /// Not set: there is always enough space.
        std::function<bool(std::uint64_t)> make_space;
    };

    /**
     * Downloads one URL into the cache directory. Rules:
     *
     * - HEAD first, redirects are followed up to max_redirects, then GET only if the file changed.
     * - Data is written to <name>.part. Only a complete download is renamed to <name>, or to <name>.ready if <name>
     *   exists (it may be playing right now).
     * - On every error the existing cached file stays untouched and is returned in local_path, so the player
     *   continues playing it.
     *
     * One job at a time; a Downloader can be reused for the next job after it finished.
     * The host drives the transport by HttpTransport::poll() and calls Downloader::poll() for the idle timeout.
     */
    class Downloader final : private HttpResponseHandler
    {
        public:
            using FinishedCallback = std::function<void(const DownloadResult &)>;

            Downloader(HttpTransport &transport, common::FileSystem &fs, Inventory &inventory,
                       const common::Clock &clock, DownloaderConfig config);
            ~Downloader() override;

            Downloader(const Downloader &)            = delete;
            Downloader &operator=(const Downloader &) = delete;

            /// Starts a job. file_name: fixed name in the cache (e.g. "index.smil"), empty = MD5 of the URL + extension.
            /// A file with a fixed name is a system file and taken with any content type; media are checked.
            /// on_finished is called exactly once, always from inside a poll(). Returns false if a job is running.
            bool start(std::string url, std::string file_name, FinishedCallback on_finished);

            /// Checks the idle timeout.
            void poll();

            /// Cancels the running job without calling on_finished.
            void cancel();

            bool isBusy() const noexcept { return phase != Phase::Idle; }

        private:
            enum class Phase
            {
                Idle,
                Head,
                Get
            };

            void onHead(const HttpResponseHead &head) override;
            bool onData(std::span<const std::byte> chunk) override;
            void onComplete(std::expected<void, TransportError> result) override;

            void sendHead();
            void sendGet();
            void evaluateHead();
            void finishGet();
            void finish(DownloadStatus status, std::string error = {});
            void closeFile();

            std::string targetPath() const;
            std::string partPath() const;
            std::string cachedCopy() const;

            HttpTransport       &transport;
            common::FileSystem  &fs;
            Inventory           &inventory;
            const common::Clock &clock;
            DownloaderConfig     config;

            Phase            phase      = Phase::Idle;
            RequestId        request_id = 0;
            FinishedCallback on_finished;

            // current job
            std::string                           url;         // original URL
            std::string                           current_url; // after redirects
            std::string                           file_name;   // fixed or determined after HEAD
            bool                                  fixed_name = false; // a system file: any content type
            std::optional<InventoryEntry>         known;       // inventory entry from before this job
            InventoryEntry                        remote;      // what the server reports now
            HttpResponseHead                      head;        // of the current request
            int                                   redirects   = 0;
            int                                   http_status = 0;
            std::string                           start_error; // transport refused the first request
            std::string                           write_error;
            std::unique_ptr<common::WritableFile> file;
            std::chrono::steady_clock::time_point last_activity;
    };
}

#endif // GARLIC_CORE_DOWNLOADER_DOWNLOADER_HPP
