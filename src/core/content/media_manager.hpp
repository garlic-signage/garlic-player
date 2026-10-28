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

#include "common/file_system.hpp"
#include "downloader/download_queue.hpp"
#include "downloader/inventory.hpp"
#include "logger/logger.hpp"
#include "smil/engine.hpp"

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
     */
    class MediaManager final : public smil::MediaSource
    {
        public:
            /// What the player can show, given by the frontend, e.g. no video on an e-ink display.
            using Supports = std::function<bool(const smil::Media &)>;

            /// Room for an extracted widget (CacheSpace::makeSpace); not set: always enough.
            using MakeSpace = std::function<bool(std::uint64_t bytes)>;

            MediaManager(downloader::DownloadQueue &queue, const downloader::Inventory &inventory,
                         common::FileSystem &fs, logger::Logger &logger, std::string cache_dir, Supports supports,
                         MakeSpace make_space = {});

            /// A new index: registers its media and region backgrounds for download.
            void load(const smil::Document &document, std::string_view base);

            /// Result from the DownloadQueue for a medium.
            void downloaded(const downloader::DownloadResult &result);

            /// Where a src (e.g. a backgroundImage) is to be loaded from: local path or URL; nullopt if not there yet.
            std::optional<std::string> locate(std::string_view src);

            std::optional<std::string> resolve(const smil::Media &media) override;
            void                       prefetch(const smil::Media &media) override;
            void                       release(const smil::Media &media) override;

        private:
            downloader::DownloadQueue   &queue;
            const downloader::Inventory &inventory;
            common::FileSystem          &fs;
            logger::Logger              &logger;
            std::string                  cache_dir;
            Supports                     supports;
            MakeSpace                    make_space;

            std::string                           base;          // of the index, for relative src
            std::set<std::string, std::less<>>    not_cacheable; // URLs the downloader reported as NotCacheable
            std::set<std::string, std::less<>>    reported;      // MEDIA_UNSUPPORTED in this index version
            std::map<std::string, int, std::less<>> in_use;      // cache file → instances playing it
            std::map<const smil::Media *, std::string> holding;  // instance of a medium → its cache file

            std::string absolute(std::string_view src) const;
            std::optional<std::string> cachedFile(std::string_view url);
            void                       registerSrc(std::string_view src);
            void                       registerMedia(const smil::Container &container);
            void                       reportUnsupported(const smil::Media &media);
    };
}

#endif // GARLIC_CORE_CONTENT_MEDIA_MANAGER_HPP
