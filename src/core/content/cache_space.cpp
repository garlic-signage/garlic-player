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
#include "content/cache_space.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <vector>

namespace garlic::core::content
{
    namespace
    {
        /// The files and directories of one cache file, removed together.
        struct Entry
        {
            std::vector<common::DirectoryEntry>   parts;
            std::chrono::system_clock::time_point last_access{};
            bool                                  downloading = false;
        };

        bool cutSuffix(std::string &name, std::string_view suffix)
        {
            if (!name.ends_with(suffix))
                return false;
            name.resize(name.size() - suffix.size());
            return true;
        }

        /// The cache file an entry of the directory belongs to: "a.jpg.part" → "a.jpg", widget directory "w" → "w.wgt".
        std::string cacheName(const common::DirectoryEntry &entry, bool &part)
        {
            std::string name = entry.name;
            part             = cutSuffix(name, ".part");
            if (!part)
                cutSuffix(name, ".ready");
            if (entry.directory)
                name += ".wgt";
            return name;
        }

        bool isKept(std::string_view name)
        {
            return name == "index.smil" || name == "temporary.smil" || name == "QtWebEngine.wgt";
        }
    }

    CacheSpace::CacheSpace(common::FileSystem &f, downloader::Inventory &inv, logger::Logger &log, std::string cache,
                           FreeSpace free)
        : fs(f), inventory(inv), logger(log), cache_dir(std::move(cache)), free_space(std::move(free))
    {
    }

    std::uint64_t CacheSpace::needed(std::uint64_t bytes) const
    {
        std::uint64_t wanted = bytes + 10; // like garlic-lib a little more
        std::uint64_t free   = free_space ? free_space() : wanted;
        return free >= wanted ? 0 : wanted - free;
    }

    bool CacheSpace::makeSpace(std::uint64_t bytes)
    {
        std::uint64_t to_free = needed(bytes);
        if (to_free == 0)
            return true;

        std::map<std::string, Entry> entries;
        for (const common::DirectoryEntry &found : fs.list(cache_dir).value_or(std::vector<common::DirectoryEntry>{}))
        {
            bool        part  = false;
            std::string name  = cacheName(found, part);
            Entry      &entry = entries[name];
            entry.parts.push_back(found);
            // a widget directory ".part" is only there while it is extracted; ".tmp": an upload of the REST API
            entry.downloading = entry.downloading || (part && !found.directory) || found.name.ends_with(".tmp");
            if (auto accessed = fs.lastAccess(cache_dir + "/" + found.name); accessed)
                entry.last_access = std::max(entry.last_access, *accessed);
        }

        std::vector<std::pair<std::string, Entry>> order;
        for (auto &[name, entry] : entries)
            if (!isKept(name) && !entry.downloading)
                order.emplace_back(name, std::move(entry));
        std::ranges::stable_sort(order, {}, [](const auto &e) { return e.second.last_access; });

        std::uint64_t removed = 0;
        for (const auto &[name, entry] : order)
        {
            if (removed >= to_free)
                break;
            for (const common::DirectoryEntry &part : entry.parts)
            {
                std::string   path          = cache_dir + "/" + part.name;
                std::uint64_t bytes_of_part = size(path, part.directory);
                auto          gone          = part.directory ? fs.removeAll(path) : fs.remove(path);
                if (gone)
                    removed += bytes_of_part;
                else
                    logger.warning("ContentManager", path + " cannot be removed: " + gone.error().message);
            }
            inventory.deleteByCacheName(name);
            logger.event(logger::Severity::Informational, "ContentManager", "OBJECT_REMOVED",
                         {{"resourceURI", cache_dir + "/" + name + " removed"}});
        }

        if (removed < to_free)
        {
            logger.event(logger::Severity::Error, "ContentManager", "FAILED_FREEING_SPACE",
                         {{"errorMessage", " - Only " + std::to_string(removed) + " bytes from needed " +
                                               std::to_string(to_free) + " bytes could be deleted"}});
            return false;
        }
        logger.event(logger::Severity::Informational, "ContentManager", "OBJECTS_REMOVED",
                     {{"information", std::to_string(removed) + " Bytes of old content were deleted"}});
        return true;
    }

    std::uint64_t CacheSpace::size(const std::string &path, bool directory) const
    {
        if (!directory)
            return fs.fileSize(path).value_or(0);
        std::uint64_t total = 0;
        for (const common::DirectoryEntry &entry : fs.list(path).value_or(std::vector<common::DirectoryEntry>{}))
            total += size(path + "/" + entry.name, entry.directory);
        return total;
    }
}
