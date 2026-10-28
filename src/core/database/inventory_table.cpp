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
#include "database/inventory_table.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <format>

namespace garlic::core::database
{
    using downloader::InventoryEntry;
    using downloader::InventoryState;

    namespace
    {
        // state values of garlic-lib: 1 TRANSFER, 2 STREAMING, 3 CHECKSUM, 4 CHECKSUM_ERROR, 5 COMPLETE
        constexpr int state_transfer = 1;
        constexpr int state_complete = 5;

        constexpr std::string_view columns =
            "resource_uri, cache_name, etag, last_modified, content_type, content_length, state, last_update";

        /// Prepared statement, finalized automatically.
        class Statement
        {
            public:
                Statement(sqlite3 *db, std::string_view sql)
                {
                    sqlite3_prepare_v2(db, sql.data(), static_cast<int>(sql.size()), &stmt, nullptr);
                }

                ~Statement() { sqlite3_finalize(stmt); }

                Statement(const Statement &)            = delete;
                Statement &operator=(const Statement &) = delete;

                bool valid() const noexcept { return stmt != nullptr; }

                void bind(int index, std::string_view value)
                {
                    sqlite3_bind_text(stmt, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
                }

                void bind(int index, std::int64_t value) { sqlite3_bind_int64(stmt, index, value); }

                int step() { return sqlite3_step(stmt); }

                std::string text(int column) const
                {
                    const unsigned char *value = sqlite3_column_text(stmt, column);
                    return value == nullptr ? std::string{} : std::string(reinterpret_cast<const char *>(value));
                }

                std::int64_t integer(int column) const { return sqlite3_column_int64(stmt, column); }

            private:
                sqlite3_stmt *stmt = nullptr;
        };

        InventoryRow readRow(const Statement &s)
        {
            InventoryRow row;
            row.url            = s.text(0);
            row.file_name      = s.text(1);
            row.etag           = s.text(2);
            row.last_modified  = s.text(3);
            row.content_type   = s.text(4);
            row.content_length = static_cast<std::uint64_t>(s.integer(5));
            row.state          = s.integer(6) == state_complete ? InventoryState::Complete : InventoryState::Transfer;
            row.last_update    = s.text(7);
            return row;
        }

        /// LIKE pattern for "base_name.<anything>"; % and _ in the name are matched literally.
        std::string baseNamePattern(std::string_view base_name)
        {
            std::string pattern;
            for (char c : base_name)
            {
                if (c == '%' || c == '_' || c == '\\')
                    pattern.push_back('\\');
                pattern.push_back(c);
            }
            return pattern + ".%";
        }
    }

    InventoryTable::InventoryTable(const common::Clock &c) : clock(c)
    {
    }

    InventoryTable::~InventoryTable()
    {
        close();
    }

    std::expected<void, std::string> InventoryTable::open(const std::string &file_path)
    {
        close();
        if (sqlite3_open(file_path.c_str(), &db) != SQLITE_OK)
        {
            last_error = file_path + ": " + sqlite3_errmsg(db);
            close();
            return std::unexpected(last_error);
        }

        // Tables of garlic-lib without last_modified are recreated. They are empty anyway, because replace() in
        // garlic-lib never wrote anything (insert and update were swapped).
        if (hasColumn("resource_uri") && !hasColumn("last_modified") && !exec("DROP TABLE inventory"))
            return std::unexpected(last_error);

        if (!hasColumn("resource_uri") && !createTable())
            return std::unexpected(last_error);

        return {};
    }

    std::optional<InventoryEntry> InventoryTable::find(std::string_view url) const
    {
        auto rows = query(std::format("SELECT {} FROM inventory WHERE resource_uri = ?1", columns), {std::string(url)});
        if (rows.empty())
            return std::nullopt;
        return rows.front();
    }

    void InventoryTable::save(const InventoryEntry &entry)
    {
        if (db == nullptr)
            return;

        // A fixed file name (index.smil) can move to another URL: the old entry has to go, cache_name is unique.
        exec("BEGIN");
        {
            Statement remove(db, "DELETE FROM inventory WHERE cache_name = ?1 AND resource_uri <> ?2");
            remove.bind(1, entry.file_name);
            remove.bind(2, entry.url);
            remove.step();
        }

        Statement upsert(db, std::format("INSERT INTO inventory ({}) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8) "
                                         "ON CONFLICT(resource_uri) DO UPDATE SET cache_name = excluded.cache_name, "
                                         "etag = excluded.etag, last_modified = excluded.last_modified, "
                                         "content_type = excluded.content_type, "
                                         "content_length = excluded.content_length, state = excluded.state, "
                                         "last_update = excluded.last_update",
                                         columns));
        upsert.bind(1, entry.url);
        upsert.bind(2, entry.file_name);
        upsert.bind(3, entry.etag);
        upsert.bind(4, entry.last_modified);
        upsert.bind(5, entry.content_type);
        upsert.bind(6, static_cast<std::int64_t>(entry.content_length));
        upsert.bind(7, std::int64_t{entry.state == InventoryState::Complete ? state_complete : state_transfer});
        upsert.bind(8, std::format("{:%FT%TZ}", std::chrono::floor<std::chrono::seconds>(clock.now())));

        if (upsert.step() != SQLITE_DONE)
        {
            last_error = sqlite3_errmsg(db);
            exec("ROLLBACK");
            return;
        }
        exec("COMMIT");
    }

    std::optional<InventoryRow> InventoryTable::findByCacheName(std::string_view cache_name) const
    {
        auto rows = query(std::format("SELECT {} FROM inventory WHERE cache_name = ?1", columns),
                          {std::string(cache_name)});
        if (rows.empty())
            return std::nullopt;
        return rows.front();
    }

    std::optional<InventoryRow> InventoryTable::findByCacheBaseName(std::string_view base_name) const
    {
        auto rows = query(std::format("SELECT {} FROM inventory WHERE cache_name LIKE ?1 ESCAPE '\\' LIMIT 1", columns),
                          {baseNamePattern(base_name)});
        if (rows.empty())
            return std::nullopt;
        return rows.front();
    }

    std::vector<InventoryRow> InventoryTable::findAll() const
    {
        return query(std::format("SELECT {} FROM inventory ORDER BY last_update DESC", columns), {});
    }

    std::vector<InventoryRow> InventoryTable::findPaginated(int max_results, int begin) const
    {
        // integers, so they can go directly into the SQL
        return query(std::format("SELECT {} FROM inventory ORDER BY last_update DESC LIMIT {} OFFSET {}", columns,
                                 std::max(max_results, 0), std::max(begin, 0)),
                     {});
    }

    void InventoryTable::deleteByCacheName(std::string_view cache_name)
    {
        if (db == nullptr)
            return;

        Statement s(db, "DELETE FROM inventory WHERE cache_name = ?1");
        s.bind(1, cache_name);
        if (s.step() != SQLITE_DONE)
            last_error = sqlite3_errmsg(db);
    }

    std::vector<InventoryRow> InventoryTable::query(std::string_view sql, std::vector<std::string> params) const
    {
        std::vector<InventoryRow> rows;
        if (db == nullptr)
            return rows;

        Statement s(db, sql);
        if (!s.valid())
        {
            last_error = sqlite3_errmsg(db);
            return rows;
        }

        for (std::size_t i = 0; i < params.size(); ++i)
            s.bind(static_cast<int>(i + 1), params[i]);

        int result;
        while ((result = s.step()) == SQLITE_ROW)
            rows.push_back(readRow(s));

        if (result != SQLITE_DONE)
            last_error = sqlite3_errmsg(db);
        return rows;
    }

    bool InventoryTable::exec(std::string_view sql)
    {
        char *error = nullptr;
        if (sqlite3_exec(db, std::string(sql).c_str(), nullptr, nullptr, &error) != SQLITE_OK)
        {
            last_error = error != nullptr ? error : "sqlite3_exec failed";
            sqlite3_free(error);
            return false;
        }
        return true;
    }

    bool InventoryTable::hasColumn(std::string_view column) const
    {
        Statement s(db, "SELECT 1 FROM pragma_table_info('inventory') WHERE name = ?1");
        s.bind(1, column);
        return s.step() == SQLITE_ROW;
    }

    bool InventoryTable::createTable()
    {
        return exec("CREATE TABLE inventory ("
                    "resource_uri TEXT PRIMARY KEY, "
                    "cache_name TEXT UNIQUE COLLATE NOCASE, "
                    "content_type TEXT, "
                    "content_length INTEGER, "
                    "last_update TEXT, "
                    "etag TEXT, "
                    "expires TEXT, "
                    "state INTEGER, "
                    "last_modified TEXT)");
    }

    void InventoryTable::close()
    {
        if (db != nullptr)
            sqlite3_close(db);
        db = nullptr;
    }
}
