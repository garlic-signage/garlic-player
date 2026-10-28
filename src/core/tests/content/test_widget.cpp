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
#include "content/widget.hpp"
#include "content_fixture.hpp"

using namespace garlic::core::content;
using namespace garlic::core::tests;
using namespace std::chrono_literals;

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
