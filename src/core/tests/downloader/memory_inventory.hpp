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
#ifndef GARLIC_CORE_TESTS_MEMORY_INVENTORY_HPP
#define GARLIC_CORE_TESTS_MEMORY_INVENTORY_HPP

#include "downloader/inventory.hpp"

#include <functional>
#include <map>

namespace garlic::core::downloader::tests
{
    class MemoryInventory final : public Inventory
    {
        public:
            std::optional<InventoryEntry> find(std::string_view url) const override
            {
                auto it = entries.find(url);
                if (it == entries.end())
                    return std::nullopt;
                return it->second;
            }

            void save(const InventoryEntry &entry) override { entries.insert_or_assign(entry.url, entry); }

            std::map<std::string, InventoryEntry, std::less<>> entries;
    };
}

#endif // GARLIC_CORE_TESTS_MEMORY_INVENTORY_HPP
