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

#include "fake_clock.hpp"
#include "common/memory_file_system.hpp"
#include "common/std_file_system.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>

using namespace garlic::core::common;
using garlic::core::tests::FakeClock;
using namespace std::chrono_literals;

namespace
{
    // Every implementation runs through the same tests. A fixture provides the file system, paths and a way to read
    // a file back, because reading is not part of the interface yet.

    struct MemoryFixture
    {
        FakeClock        clock;
        MemoryFileSystem fs{clock};

        std::string path(std::string_view name) const { return "cache/" + std::string(name); }
        std::string read(std::string_view p) const { return fs.content(p).value_or("<missing>"); }
    };

    struct StdFixture
    {
        StdFileSystem fs;
        std::string   dir;

        StdFixture()
        {
            std::random_device random;
            auto               base = std::filesystem::temp_directory_path() /
                        ("garlic-core-test-" + std::to_string(random()) + std::to_string(random()));
            dir = base.generic_string();
            (void)fs.createDirectories(dir);
        }

        ~StdFixture()
        {
            std::error_code ec;
            std::filesystem::remove_all(dir, ec);
        }

        std::string path(std::string_view name) const { return dir + "/" + std::string(name); }

        std::string read(std::string_view p) const
        {
            std::ifstream file{std::string(p), std::ios::binary};
            if (!file)
                return "<missing>";
            return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        }
    };

    std::span<const std::byte> bytes(std::string_view text)
    {
        return std::as_bytes(std::span(text.data(), text.size()));
    }

    template <typename Fixture> void writeFile(Fixture &f, const std::string &path, std::string_view text, WriteMode mode)
    {
        auto file = f.fs.openForWrite(path, mode);
        REQUIRE(file.has_value());
        REQUIRE((*file)->write(bytes(text)).has_value());
        REQUIRE((*file)->close().has_value());
    }
}

TEST_CASE_TEMPLATE("FileSystem writes, truncates and appends", Fixture, MemoryFixture, StdFixture)
{
    Fixture     f;
    std::string file = f.path("a.jpg.part");

    CHECK_FALSE(f.fs.exists(file));

    writeFile(f, file, "Hello", WriteMode::Truncate);
    CHECK(f.fs.exists(file));
    CHECK(f.fs.fileSize(file) == 5u);
    CHECK(f.read(file) == "Hello");

    // resume of a download
    writeFile(f, file, " World", WriteMode::Append);
    CHECK(f.read(file) == "Hello World");

    writeFile(f, file, "New", WriteMode::Truncate);
    CHECK(f.read(file) == "New");
}

TEST_CASE_TEMPLATE("FileSystem writes in several chunks", Fixture, MemoryFixture, StdFixture)
{
    Fixture f;
    auto    file = f.fs.openForWrite(f.path("chunks"), WriteMode::Truncate);
    REQUIRE(file.has_value());

    for (std::string_view chunk : {"01", "23", "45"})
        REQUIRE((*file)->write(bytes(chunk)).has_value());
    REQUIRE((*file)->close().has_value());

    CHECK(f.read(f.path("chunks")) == "012345");
}

TEST_CASE_TEMPLATE("FileSystem rejects writes after close()", Fixture, MemoryFixture, StdFixture)
{
    Fixture f;
    auto    file = f.fs.openForWrite(f.path("closed"), WriteMode::Truncate);
    REQUIRE(file.has_value());
    REQUIRE((*file)->close().has_value());

    CHECK_FALSE((*file)->write(bytes("x")).has_value());
    CHECK((*file)->close().has_value()); // closing twice is harmless
}

TEST_CASE_TEMPLATE("FileSystem rename replaces an existing target", Fixture, MemoryFixture, StdFixture)
{
    Fixture f;
    writeFile(f, f.path("a.jpg"), "old", WriteMode::Truncate);
    writeFile(f, f.path("a.jpg.part"), "new", WriteMode::Truncate);

    REQUIRE(f.fs.rename(f.path("a.jpg.part"), f.path("a.jpg")).has_value());

    CHECK_FALSE(f.fs.exists(f.path("a.jpg.part")));
    CHECK(f.read(f.path("a.jpg")) == "new");
}

TEST_CASE_TEMPLATE("FileSystem reports missing files as NotFound", Fixture, MemoryFixture, StdFixture)
{
    Fixture     f;
    std::string missing = f.path("missing");

    CHECK(f.fs.fileSize(missing).error().code == FileSystemErrorCode::NotFound);
    CHECK(f.fs.lastModified(missing).error().code == FileSystemErrorCode::NotFound);
    CHECK(f.fs.remove(missing).error().code == FileSystemErrorCode::NotFound);
    CHECK(f.fs.rename(missing, f.path("other")).error().code == FileSystemErrorCode::NotFound);
}

TEST_CASE_TEMPLATE("FileSystem removes files", Fixture, MemoryFixture, StdFixture)
{
    Fixture f;
    writeFile(f, f.path("a.jpg"), "data", WriteMode::Truncate);

    REQUIRE(f.fs.remove(f.path("a.jpg")).has_value());
    CHECK_FALSE(f.fs.exists(f.path("a.jpg")));
}

TEST_CASE_TEMPLATE("FileSystem creates nested directories", Fixture, MemoryFixture, StdFixture)
{
    Fixture f;
    REQUIRE(f.fs.createDirectories(f.path("x/y/z")).has_value());
    REQUIRE(f.fs.createDirectories(f.path("x/y/z")).has_value()); // existing is no error

    writeFile(f, f.path("x/y/z/file"), "data", WriteMode::Truncate);
    CHECK(f.read(f.path("x/y/z/file")) == "data");
}

TEST_CASE("StdFileSystem handles UTF-8 paths")
{
    StdFixture  f;
    std::string file = f.path("Größe_µ.jpg");

    writeFile(f, file, "data", WriteMode::Truncate);
    CHECK(f.fs.exists(file));
    CHECK(f.fs.fileSize(file) == 4u);
}

TEST_CASE("StdFileSystem lastModified is close to the current time")
{
    StdFixture f;
    writeFile(f, f.path("a"), "data", WriteMode::Truncate);

    auto modified = f.fs.lastModified(f.path("a"));
    REQUIRE(modified.has_value());

    auto difference = std::chrono::system_clock::now() - *modified;
    CHECK(difference < 60s);
    CHECK(difference > -60s);
}

TEST_CASE("MemoryFileSystem takes lastModified from the clock")
{
    MemoryFixture f;
    writeFile(f, f.path("a"), "data", WriteMode::Truncate);
    auto first = f.fs.lastModified(f.path("a"));

    f.clock.advance(10s);
    writeFile(f, f.path("a"), "more", WriteMode::Append);

    REQUIRE(first.has_value());
    CHECK(f.fs.lastModified(f.path("a")) == *first + 10s);
}
