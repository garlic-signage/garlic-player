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
#ifndef GARLIC_CORE_DATABASE_INVENTORY_TABLE_HPP
#define GARLIC_CORE_DATABASE_INVENTORY_TABLE_HPP

#include "common/clock.hpp"
#include "downloader/inventory.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace garlic::core::database
{
    /// Inventory entry with the time of the last change (ISO 8601 UTC), e.g. for the inventory report.
    struct InventoryRow : downloader::InventoryEntry
    {
        std::string last_update;
    };

    /**
     * Table "inventory" in the SQLite file garlic.db: which URL is stored under which file name in the cache.
     * Same table as in garlic-lib, plus the column last_modified (Last-Modified of the server).
     */
    class InventoryTable final : public downloader::Inventory
    {
        public:
            explicit InventoryTable(const common::Clock &clock);
            ~InventoryTable() override;

            InventoryTable(const InventoryTable &)            = delete;
            InventoryTable &operator=(const InventoryTable &) = delete;

            /// Opens or creates the database file and the table.
            std::expected<void, std::string> open(const std::string &file_path);

            // downloader::Inventory
            std::optional<downloader::InventoryEntry> find(std::string_view url) const override;
            void                                      save(const downloader::InventoryEntry &entry) override;

            /// Like save(), with the time of the last change given (ISO 8601), e.g. modifiedDate of an upload.
            void save(const downloader::InventoryEntry &entry, std::string_view last_update);

            std::optional<InventoryRow> findByCacheName(std::string_view cache_name) const;

            /// Entry whose file name is base_name plus an extension, e.g. "abc" finds "abc.jpg" (REST API files/{id}).
            std::optional<InventoryRow> findByCacheBaseName(std::string_view base_name) const;

            /// All entries, newest first.
            std::vector<InventoryRow> findAll() const;
            std::vector<InventoryRow> findPaginated(int max_results, int begin) const;

            void deleteByCacheName(std::string_view cache_name);

            /// Message of the last failed operation, for logging.
            const std::string &lastError() const noexcept { return last_error; }

        private:
            std::vector<InventoryRow> query(std::string_view sql, std::vector<std::string> params) const;
            bool                      exec(std::string_view sql);
            bool                      hasColumn(std::string_view column) const;
            bool                      createTable();
            void                      close();

            const common::Clock &clock;
            sqlite3             *db = nullptr;
            mutable std::string  last_error;
    };
}

#endif // GARLIC_CORE_DATABASE_INVENTORY_TABLE_HPP
