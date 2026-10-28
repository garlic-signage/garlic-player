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
#ifndef GARLIC_CORE_CONTENT_CACHE_SPACE_HPP
#define GARLIC_CORE_CONTENT_CACHE_SPACE_HPP

#include "common/file_system.hpp"
#include "downloader/inventory.hpp"
#include "logger/logger.hpp"

#include <cstdint>
#include <functional>
#include <string>

namespace garlic::core::content
{
    /**
     * Makes room in the cache when the disk is full, garlic-lib: FreeDiscSpace. Called before a download, before a
     * widget is extracted and before an upload of the REST API is stored.
     *
     * The entries of the cache directory are removed, the longest unread first, until the new bytes fit. An entry is
     * a file with its ".part" and ".ready" and, for a widget, the ".wgt" with its directory; its inventory row goes
     * too. Kept are the index (index.smil, temporary.smil), the web cache of the Qt frontends (QtWebEngine), files
     * that are being downloaded (".part") and uploads of the REST API (".tmp"). Media of the current index can go like in garlic-lib: they are downloaded
     * again when they are needed.
     */
    class CacheSpace
    {
        public:
            /// Free bytes of the cache's disk (system::diskSpace: 4 % stay free for the operating system).
            using FreeSpace = std::function<std::uint64_t()>;

            CacheSpace(common::FileSystem &fs, downloader::Inventory &inventory, logger::Logger &logger,
                       std::string cache_dir, FreeSpace free_space);

            /// Bytes to remove so that `bytes` fit, 0 if they fit (garlic-lib: calculateNeededDiscSpaceToFree).
            std::uint64_t needed(std::uint64_t bytes) const;

            /// Removes entries until `bytes` fit. False if not enough could be removed.
            bool makeSpace(std::uint64_t bytes);

        private:
            common::FileSystem    &fs;
            downloader::Inventory &inventory;
            logger::Logger        &logger;
            std::string            cache_dir;
            FreeSpace              free_space;

            std::uint64_t size(const std::string &path, bool directory) const;
    };
}

#endif // GARLIC_CORE_CONTENT_CACHE_SPACE_HPP
