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
#ifndef GARLIC_CORE_CONTENT_MEDIA_MANAGER_HPP
#define GARLIC_CORE_CONTENT_MEDIA_MANAGER_HPP

#include "common/clock.hpp"
#include "common/file_system.hpp"
#include "downloader/download_queue.hpp"
#include "downloader/inventory.hpp"
#include "logger/logger.hpp"
#include "smil/engine.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace garlic::core::content
{
    /**
     * The media of the index, garlic-lib: MediaManager and MediaModel. Downloads them into the cache and tells the
     * engine where they are (smil::MediaSource).
     *
     *   remote (http, https, ftp)  downloaded; a version in the cache plays at once, also offline
     *   relative                   against the base of the index, then like above
     *   local path, data:, adapi:  used as they are
     *   not supported              not downloaded, MEDIA_UNSUPPORTED once per index version
     *
     * A file of the index that is missing in the cache (removed by hand, by the REST API, ...) is downloaded again
     * when the engine asks for it. After a failed download the URL waits IndexManager::retry; a download that failed
     * for lack of space (DISK_FULL) waits for the next index.
     */
    class MediaManager final : public smil::MediaSource
    {
        public:
            /// What the player can show, given by the frontend, e.g. no video on an e-ink display.
            using Supports = std::function<bool(const smil::Media &)>;

            /// Room for an extracted widget (CacheSpace::makeSpace); not set: always enough.
            using MakeSpace = std::function<bool(std::uint64_t bytes)>;

            MediaManager(downloader::DownloadQueue &queue, const downloader::Inventory &inventory,
                         common::FileSystem &fs, const common::Clock &clock, logger::Logger &logger,
                         std::string cache_dir, Supports supports, MakeSpace make_space = {});

            /// A new index: registers its media and region backgrounds for download.
            void load(const smil::Document &document, std::string_view base);

            /// Result from the DownloadQueue for a medium.
            void downloaded(const downloader::DownloadResult &result);

            /// Where a src (e.g. a backgroundImage) is to be loaded from: local path or URL; nullopt if not there yet.
            std::optional<std::string> locate(std::string_view src);

            /// After clear_cache until the restart: missing files are not downloaded again.
            void holdDownloads() noexcept { downloads_held = true; }

            /// The cache file names of the remote files of the current index, kept by CacheSpace.
            std::set<std::string> currentFiles() const;

            /// A remote file of the current index (medium or background image), not the index or a task file
            bool isMedia(std::string_view url) const { return current.contains(url); }

            /// For the debug display: the title of the medium from the SMIL, otherwise the file name of the URL
            std::string displayName(std::string_view url) const;

            std::optional<std::string> resolve(const smil::Media &media) override;
            void                       prefetch(const smil::Media &media) override;
            void                       release(const smil::Media &media) override;

        private:
            downloader::DownloadQueue   &queue;
            const downloader::Inventory &inventory;
            common::FileSystem          &fs;
            const common::Clock         &clock;
            logger::Logger              &logger;
            std::string                  cache_dir;
            Supports                     supports;
            MakeSpace                    make_space;

            std::string                           base;          // of the index, for relative src
            std::set<std::string, std::less<>>    not_cacheable; // URLs the downloader reported as NotCacheable
            std::set<std::string, std::less<>>    reported;      // MEDIA_UNSUPPORTED in this index version
            std::set<std::string, std::less<>>    current;       // remote URLs of the index
            std::map<std::string, std::string, std::less<>> titles; // remote URL → title of its medium, if it has one
            std::map<std::string, std::chrono::steady_clock::time_point, std::less<>> retry_after; // failed downloads
            bool downloads_held = false; // holdDownloads()
            std::map<std::string, int, std::less<>> in_use;      // cache file → instances playing it
            std::map<const smil::Media *, std::string> holding;  // instance of a medium → its cache file

            std::string absolute(std::string_view src) const;
            std::optional<std::string> cachedFile(std::string_view url);
            void                       download(std::string_view url);
            void                       registerSrc(std::string_view src);
            void                       registerMedia(const smil::Container &container, bool audio_video);
            void                       reportUnsupported(const smil::Media &media);
    };
}

#endif // GARLIC_CORE_CONTENT_MEDIA_MANAGER_HPP
