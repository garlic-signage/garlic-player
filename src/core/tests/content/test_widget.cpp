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
#include "common/std_file_system.hpp"
#include "content/widget.hpp"
#include "content_fixture.hpp"

#include <filesystem>
#include <random>

using namespace garlic::core::content;
using namespace garlic::core::tests;
using namespace std::chrono_literals;

namespace
{
    /// A zip whose entry "placeholder" is renamed to name afterwards: miniz does not write unsafe names itself.
    std::string zipWithName(const std::string &placeholder, const std::string &name)
    {
        REQUIRE(placeholder.size() == name.size());
        std::string zip = makeZip({{"index.html", "x"}, {placeholder, "bad"}});
        for (auto pos = zip.find(placeholder); pos != std::string::npos; pos = zip.find(placeholder, pos + 1))
            zip.replace(pos, name.size(), name);
        return zip;
    }
}

TEST_CASE("widget paths: the directory next to the file, index.html inside")
{
    CHECK(widgetDirectory("cache/abc.wgt") == "cache/abc");
    CHECK(widgetDirectory("cache/xyz.secret.wgt") == "cache/xyz.secret"); // garlic-lib cut at the first dot
    CHECK(widgetIndex("cache/abc.wgt") == "cache/abc/index.html");
}

TEST_CASE("extracts all files, also in subdirectories")
{
    ContentFixture f;
    writeFile(f.fs, "cache/clock.wgt", makeZip({{"index.html", "<html>"}, {"js/app.js", "code"}, {"img/", ""}}));

    REQUIRE(extractWidget(f.fs, "cache/clock.wgt").has_value());
    CHECK(f.fs.content("cache/clock/index.html") == "<html>");
    CHECK(f.fs.content("cache/clock/js/app.js") == "code");
    CHECK_FALSE(f.fs.exists("cache/clock.part"));
}

TEST_CASE("extracts again only if the file is newer than the directory")
{
    ContentFixture f;
    writeFile(f.fs, "cache/xyz.secret.wgt", makeZip({{"index.html", "v1"}}));
    REQUIRE(extractWidget(f.fs, "cache/xyz.secret.wgt").has_value());

    f.clock.advance(1s);
    writeFile(f.fs, "cache/xyz.secret/index.html", "touched"); // a second check must not extract again
    REQUIRE(extractWidget(f.fs, "cache/xyz.secret.wgt").has_value());
    CHECK(f.fs.content("cache/xyz.secret/index.html") == "touched");

    f.clock.advance(1s);
    writeFile(f.fs, "cache/xyz.secret.wgt", makeZip({{"index.html", "v2"}}));
    REQUIRE(extractWidget(f.fs, "cache/xyz.secret.wgt").has_value());
    CHECK(f.fs.content("cache/xyz.secret/index.html") == "v2");
}

TEST_CASE("a broken widget keeps the old version")
{
    ContentFixture f;
    writeFile(f.fs, "cache/w.wgt", makeZip({{"index.html", "old"}}));
    REQUIRE(extractWidget(f.fs, "cache/w.wgt").has_value());

    f.clock.advance(1s);
    writeFile(f.fs, "cache/w.wgt", "no zip at all");
    CHECK_FALSE(extractWidget(f.fs, "cache/w.wgt").has_value());
    CHECK(f.fs.content("cache/w/index.html") == "old");
    CHECK_FALSE(f.fs.exists("cache/w.part"));
}

TEST_CASE("zip slip: an entry outside the directory rejects the whole widget")
{
    ContentFixture f;
    writeFile(f.fs, "cache/evil.wgt", makeZip({{"index.html", "x"}, {"js/../../config.ini", "bad"}}));

    auto result = extractWidget(f.fs, "cache/evil.wgt");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().contains("unsafe path"));
    CHECK_FALSE(f.fs.exists("config.ini"));
    CHECK_FALSE(f.fs.exists("cache/config.ini"));
    CHECK_FALSE(f.fs.exists("cache/evil"));
}

TEST_CASE("zip slip: absolute paths, drive letters and backslashes are rejected too")
{
    for (auto [placeholder, name] : {std::pair{"Xabs.txt", "/abs.txt"}, std::pair{"Xback.txt", "\\back.txt"},
                                     std::pair{"CXx.txt", "C:x.txt"}, std::pair{"aXYYXx.txt", "a\\..\\x.txt"}})
    {
        CAPTURE(name);
        ContentFixture f;
        writeFile(f.fs, "cache/evil.wgt", zipWithName(placeholder, name));

        auto result = extractWidget(f.fs, "cache/evil.wgt");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().contains("unsafe path"));
        CHECK_FALSE(f.fs.exists("cache/evil"));
    }
}

TEST_CASE("not extracted again if the directory is exactly as old as the file")
{
    ContentFixture f;
    writeFile(f.fs, "cache/w.wgt", makeZip({{"index.html", "v1"}}));
    REQUIRE(extractWidget(f.fs, "cache/w.wgt").has_value());
    writeFile(f.fs, "cache/w/index.html", "touched"); // same time as the file
    REQUIRE(extractWidget(f.fs, "cache/w.wgt").has_value());
    CHECK(f.fs.content("cache/w/index.html") == "touched");
}

TEST_CASE("a new version replaces the whole directory, a left over .part does not get into it")
{
    ContentFixture f;
    writeFile(f.fs, "cache/w.wgt", makeZip({{"index.html", "v1"}, {"old.js", "old"}}));
    REQUIRE(extractWidget(f.fs, "cache/w.wgt").has_value());

    f.clock.advance(1s);
    writeFile(f.fs, "cache/w.part/stale.js", "stale"); // a broken off extraction
    writeFile(f.fs, "cache/w.wgt", makeZip({{"index.html", "v2"}}));
    REQUIRE(extractWidget(f.fs, "cache/w.wgt").has_value());
    CHECK(f.fs.content("cache/w/index.html") == "v2");
    CHECK_FALSE(f.fs.exists("cache/w/old.js"));
    CHECK_FALSE(f.fs.exists("cache/w/stale.js"));
}

TEST_CASE("on the real file system: subdirectories and empty directories are created")
{
    std::random_device random;
    auto dir = std::filesystem::temp_directory_path() / ("garlic-widget-" + std::to_string(random()));
    garlic::core::common::StdFileSystem fs;
    std::string wgt = (dir / "clock.wgt").generic_string();
    REQUIRE(fs.createDirectories(dir.generic_string()).has_value());
    writeFile(fs, wgt, makeZip({{"index.html", "<html>"}, {"js/lib/app.js", "code"}, {"img/", ""}}));

    auto result = extractWidget(fs, wgt);
    CHECK(result.has_value());
    CHECK(fs.readFile((dir / "clock/js/lib/app.js").generic_string()) == "code");
    CHECK(std::filesystem::is_directory(dir / "clock/img"));
    std::filesystem::remove_all(dir);
}
