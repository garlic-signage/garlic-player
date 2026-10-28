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
#include <doctest/doctest.h>

#include "database/inventory_table.hpp"
#include "fake_clock.hpp"

#include <sqlite3.h>

#include <filesystem>
#include <random>

using namespace garlic::core::database;
using garlic::core::downloader::InventoryEntry;
using garlic::core::downloader::InventoryState;
using garlic::core::tests::FakeClock;
using namespace std::chrono_literals;

namespace
{
    InventoryEntry entry(std::string url, std::string file_name, std::string etag = "\"v1\"")
    {
        return {std::move(url), std::move(file_name), std::move(etag), "Wed, 01 Jan 2025 10:00:00 GMT",
                "image/jpeg", 1234, InventoryState::Complete};
    }

    struct Fixture
    {
        FakeClock      clock;
        InventoryTable table{clock};

        Fixture() { REQUIRE(table.open(":memory:").has_value()); }
    };

    /// Database file in the temp directory, removed at the end.
    struct TempFile
    {
        std::string path;

        TempFile()
        {
            std::random_device random;
            path = (std::filesystem::temp_directory_path() / ("garlic-test-" + std::to_string(random()) + ".db"))
                       .string();
        }

        ~TempFile()
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    };

    void execRaw(const std::string &path, const char *sql)
    {
        sqlite3 *db = nullptr;
        REQUIRE(sqlite3_open(path.c_str(), &db) == SQLITE_OK);
        CHECK(sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
        sqlite3_close(db);
    }
}

TEST_CASE("save and find return all fields")
{
    Fixture f;
    f.table.save(entry("http://example.com/a.jpg", "abc.jpg"));

    auto found = f.table.find("http://example.com/a.jpg");
    REQUIRE(found.has_value());
    CHECK(found->file_name == "abc.jpg");
    CHECK(found->etag == "\"v1\"");
    CHECK(found->last_modified == "Wed, 01 Jan 2025 10:00:00 GMT");
    CHECK(found->content_type == "image/jpeg");
    CHECK(found->content_length == 1234);
    CHECK(found->state == InventoryState::Complete);

    CHECK_FALSE(f.table.find("http://example.com/other.jpg").has_value());
}

TEST_CASE("save updates an existing entry (was broken in garlic-lib)")
{
    Fixture f;
    auto    e = entry("http://example.com/a.jpg", "abc.jpg");
    e.state   = InventoryState::Transfer;
    f.table.save(e);

    e.etag  = "\"v2\"";
    e.state = InventoryState::Complete;
    f.table.save(e);

    auto found = f.table.find("http://example.com/a.jpg");
    REQUIRE(found.has_value());
    CHECK(found->etag == "\"v2\"");
    CHECK(found->state == InventoryState::Complete);
    CHECK(f.table.findAll().size() == 1);
}

TEST_CASE("quotes in URLs do not break the SQL")
{
    Fixture     f;
    std::string url = "http://example.com/it's a file.jpg?x='1' OR 1=1--";
    f.table.save(entry(url, "abc.jpg"));

    CHECK(f.table.find(url).has_value());
    CHECK_FALSE(f.table.find("' OR 1=1--").has_value());
}

TEST_CASE("a fixed file name moving to another URL replaces the old entry")
{
    Fixture f;
    f.table.save(entry("http://example.com/index.smil", "index.smil"));
    f.table.save(entry("http://other.example.com/index.smil", "index.smil"));

    CHECK_FALSE(f.table.find("http://example.com/index.smil").has_value());
    CHECK(f.table.find("http://other.example.com/index.smil").has_value());
    CHECK(f.table.findByCacheName("index.smil")->url == "http://other.example.com/index.smil");
}

TEST_CASE("findByCacheName")
{
    Fixture f;
    f.table.save(entry("http://example.com/a.jpg", "abc.jpg"));

    CHECK(f.table.findByCacheName("abc.jpg")->url == "http://example.com/a.jpg");
    CHECK_FALSE(f.table.findByCacheName("abc").has_value());
}

TEST_CASE("findByCacheBaseName finds the name with any extension")
{
    Fixture f;
    f.table.save(entry("http://example.com/a.jpg", "abc.jpg"));
    f.table.save(entry("http://example.com/b.jpg", "abcd.png"));

    CHECK(f.table.findByCacheBaseName("abc")->file_name == "abc.jpg");
    CHECK(f.table.findByCacheBaseName("abcd")->file_name == "abcd.png");
    CHECK_FALSE(f.table.findByCacheBaseName("ab").has_value());
    CHECK_FALSE(f.table.findByCacheBaseName("a%").has_value()); // wildcards are literal
}

TEST_CASE("findAll returns the newest entries first, with last_update")
{
    Fixture f;
    f.table.save(entry("http://example.com/1.jpg", "1.jpg"));
    f.clock.advance(10s);
    f.table.save(entry("http://example.com/2.jpg", "2.jpg"));

    auto rows = f.table.findAll();
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].file_name == "2.jpg");
    CHECK(rows[0].last_update == "2026-01-01T00:00:10Z");
    CHECK(rows[1].last_update == "2026-01-01T00:00:00Z");
}

TEST_CASE("findPaginated")
{
    Fixture f;
    for (int i = 0; i < 5; ++i)
    {
        f.table.save(entry("http://example.com/" + std::to_string(i) + ".jpg", std::to_string(i) + ".jpg"));
        f.clock.advance(1s);
    }

    auto page = f.table.findPaginated(2, 1);
    REQUIRE(page.size() == 2);
    CHECK(page[0].file_name == "3.jpg");
    CHECK(page[1].file_name == "2.jpg");
    CHECK(f.table.findPaginated(10, 4).size() == 1);
}

TEST_CASE("deleteByCacheName")
{
    Fixture f;
    f.table.save(entry("http://example.com/a.jpg", "abc.jpg"));
    f.table.deleteByCacheName("abc.jpg");

    CHECK_FALSE(f.table.find("http://example.com/a.jpg").has_value());
}

TEST_CASE("entries survive reopening the file")
{
    TempFile  file;
    FakeClock clock;
    {
        InventoryTable table{clock};
        REQUIRE(table.open(file.path).has_value());
        table.save(entry("http://example.com/a.jpg", "abc.jpg"));
    }

    InventoryTable table{clock};
    REQUIRE(table.open(file.path).has_value());
    CHECK(table.find("http://example.com/a.jpg").has_value());
}

TEST_CASE("a garlic-lib table without last_modified is recreated")
{
    TempFile file;
    execRaw(file.path, "CREATE TABLE inventory (resource_uri TEXT PRIMARY KEY, cache_name TEXT UNIQUE, "
                       "content_type TEXT, content_length INTEGER, last_update TEXT, etag TEXT, expires TEXT, "
                       "state INTEGER);"
                       "INSERT INTO inventory VALUES ('http://example.com/a.jpg', 'abc.jpg', 'image/jpeg', 4, "
                       "'Wed Oct 28 03:30:56 2026', 'v1', '', 5);");

    FakeClock      clock;
    InventoryTable table{clock};
    REQUIRE(table.open(file.path).has_value());

    CHECK(table.findAll().empty());
    table.save(entry("http://example.com/a.jpg", "abc.jpg"));
    CHECK(table.find("http://example.com/a.jpg")->last_modified == "Wed, 01 Jan 2025 10:00:00 GMT");
}

TEST_CASE("garlic-lib states other than COMPLETE count as not complete")
{
    TempFile  file;
    FakeClock clock;
    {
        InventoryTable table{clock};
        REQUIRE(table.open(file.path).has_value());
    }
    execRaw(file.path, "INSERT INTO inventory (resource_uri, cache_name, state) VALUES ('http://a', 'a.jpg', 3)");

    InventoryTable table{clock};
    REQUIRE(table.open(file.path).has_value());
    CHECK(table.find("http://a")->state == InventoryState::Transfer);
}

TEST_CASE("open fails for a path that cannot be created")
{
    FakeClock      clock;
    InventoryTable table{clock};

    auto result = table.open("/nonexistent-directory/garlic.db");
    CHECK_FALSE(result.has_value());
    CHECK_FALSE(table.find("x").has_value()); // no crash without database
}
