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
#ifndef GARLIC_CORE_DOWNLOADER_INVENTORY_HPP
#define GARLIC_CORE_DOWNLOADER_INVENTORY_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace garlic::core::downloader
{
    enum class InventoryState
    {
        Transfer, // download running or aborted
        Complete
    };

    /// What the downloader remembers about a URL. ETag and Last-Modified are stored exactly as the server sent them.
    struct InventoryEntry
    {
        std::string    url; // original URL, before redirects
        std::string    file_name; // name in the cache directory
        std::string    etag;
        std::string    last_modified;
        std::string    content_type;
        std::uint64_t  content_length = 0;
        InventoryState state          = InventoryState::Transfer;
    };

    /// Storage of the inventory entries. Implemented by the database module, in memory for the tests.
    class Inventory
    {
        public:
            virtual ~Inventory() = default;

            virtual std::optional<InventoryEntry> find(std::string_view url) const = 0;
            virtual void                          save(const InventoryEntry &entry) = 0;
    };
}

#endif // GARLIC_CORE_DOWNLOADER_INVENTORY_HPP
