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
#include "content/widget.hpp"
#include "content_fixture.hpp"

using namespace garlic::core;
using namespace garlic::core::content;
using namespace garlic::core::tests;
using namespace std::chrono_literals;

namespace
{
    /// The disk: a fixed size, the files of the cache count as used.
    struct SpaceFixture : ContentFixture
    {
        std::uint64_t disk = 100;
        CacheSpace    space{fs, inventory, logger, "cache", [this] { return disk - used(); }};

        std::uint64_t used() const
        {
            std::uint64_t bytes = 0;
            for (const char *path : {"cache/index.smil", "cache/old.jpg", "cache/new.jpg", "cache/w.wgt",
                                     "cache/w/index.html", "cache/w/js/app.js", "cache/new.jpg.ready",
                                     "cache/loading.mp4.part", "cache/upload-1.tmp"})
                bytes += fs.fileSize(path).value_or(0);
            return bytes;
        }

        /// A file in the cache with its inventory row, written now.
        void cached(const std::string &name, std::size_t size)
        {
            writeFile(fs, "cache/" + name, std::string(size, 'x'));
            downloader::InventoryEntry entry;
            entry.url       = "http://hub/" + name;
            entry.file_name = name;
            entry.state     = downloader::InventoryState::Complete;
            inventory.save(entry);
            clock.advance(1min);
        }
    };
}

TEST_CASE("needed: the bytes that do not fit, with 10 bytes to spare like garlic-lib")
{
    SpaceFixture f;
    f.cached("old.jpg", 50);
    CHECK(f.space.needed(40) == 0);
    CHECK(f.space.needed(41) == 1);
    CHECK(f.space.needed(60) == 20);
}

TEST_CASE("enough space: nothing is removed")
{
    SpaceFixture f;
    f.cached("old.jpg", 30);
    CHECK(f.space.makeSpace(50));
    CHECK(f.fs.exists("cache/old.jpg"));
    CHECK(f.events().find("OBJECT") == std::string::npos);
}

TEST_CASE("the longest unread files go first, with their inventory row, until the bytes fit")
{
    SpaceFixture f;
    f.cached("old.jpg", 30);
    f.cached("new.jpg", 30);
    f.cached("w.wgt", 20);
    (void)f.fs.readFile("cache/old.jpg"); // read again: now the newest

    REQUIRE(f.space.makeSpace(40)); // 20 free, 30 needed
    CHECK(f.fs.exists("cache/old.jpg"));
    CHECK_FALSE(f.fs.exists("cache/new.jpg"));
    CHECK(f.fs.exists("cache/w.wgt"));
    CHECK_FALSE(f.inventory.find("http://hub/new.jpg").has_value());
    CHECK(f.inventory.find("http://hub/old.jpg").has_value());
    CHECK(f.events().find("<eventName>OBJECT_REMOVED</eventName>") != std::string::npos);
    CHECK(f.events().find("cache/new.jpg removed") != std::string::npos);
    CHECK(f.events().find("<eventName>OBJECTS_REMOVED</eventName>") != std::string::npos);
}

TEST_CASE("a widget goes with its directory, a file with its waiting update")
{
    SpaceFixture f;
    f.cached("w.wgt", 10);
    writeFile(f.fs, "cache/w/index.html", std::string(20, 'h'));
    writeFile(f.fs, "cache/w/js/app.js", std::string(20, 'j'));
    f.clock.advance(1min);
    f.cached("new.jpg", 10);
    writeFile(f.fs, "cache/new.jpg.ready", std::string(10, 'r'));

    REQUIRE(f.space.makeSpace(70)); // 30 free, 50 needed: the widget (50) is enough
    CHECK_FALSE(f.fs.exists("cache/w.wgt"));
    CHECK_FALSE(f.fs.exists("cache/w"));
    CHECK_FALSE(f.inventory.find("http://hub/w.wgt").has_value());
    CHECK(f.fs.exists("cache/new.jpg"));

    REQUIRE(f.space.makeSpace(90)); // 80 free
    CHECK_FALSE(f.fs.exists("cache/new.jpg"));
    CHECK_FALSE(f.fs.exists("cache/new.jpg.ready"));
}

TEST_CASE("kept: the index, running downloads and uploads; not enough space is FAILED_FREEING_SPACE")
{
    SpaceFixture f;
    writeFile(f.fs, "cache/index.smil", std::string(20, 's'));
    writeFile(f.fs, "cache/loading.mp4.part", std::string(20, 'p'));
    writeFile(f.fs, "cache/upload-1.tmp", std::string(20, 'u'));
    f.clock.advance(1min);
    f.cached("old.jpg", 20);

    CHECK_FALSE(f.space.makeSpace(50)); // 20 free, 40 needed, only old.jpg (20) can go
    CHECK_FALSE(f.fs.exists("cache/old.jpg"));
    CHECK(f.fs.exists("cache/index.smil"));
    CHECK(f.fs.exists("cache/loading.mp4.part"));
    CHECK(f.fs.exists("cache/upload-1.tmp"));
    CHECK(f.events().find("<eventName>FAILED_FREEING_SPACE</eventName>") != std::string::npos);
    CHECK(f.events().find("Only 20 bytes from needed 40 bytes") != std::string::npos);
}

TEST_CASE("an empty cache: false, nothing to remove")
{
    SpaceFixture f;
    f.disk = 5;
    CHECK_FALSE(f.space.makeSpace(10));
}

TEST_CASE("a widget is not extracted when its unpacked size does not fit")
{
    ContentFixture f;
    writeFile(f.fs, "cache/clock.wgt", makeZip({{"index.html", std::string(1000, 'h')}}));

    std::uint64_t asked  = 0;
    auto          no     = [&](std::uint64_t bytes) { asked = bytes; return false; };
    auto          denied = extractWidget(f.fs, "cache/clock.wgt", no);
    REQUIRE_FALSE(denied.has_value());
    CHECK(denied.error() == "not enough disk space to extract the widget");
    CHECK(asked == 1000);
    CHECK_FALSE(f.fs.exists("cache/clock"));

    CHECK(extractWidget(f.fs, "cache/clock.wgt", [](std::uint64_t) { return true; }).has_value());
    CHECK(f.fs.exists("cache/clock/index.html"));
}
