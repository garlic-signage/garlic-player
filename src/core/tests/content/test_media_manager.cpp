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
#include "content/media_manager.hpp"
#include "content_fixture.hpp"
#include "smil/parser.hpp"

using namespace garlic::core;
using namespace garlic::core::content;
using namespace garlic::core::tests;
using namespace std::chrono_literals;

namespace
{
    struct Fixture : ContentFixture
    {
        smil::Document document;
        MediaManager   media{queue, inventory, fs, logger, "cache", nullptr};

        Fixture()
        {
            on_result = [this](const downloader::DownloadResult &r) { media.downloaded(r); };
        }

        void load(std::string_view body, std::string_view base = "http://hub/index/", std::string_view head = "")
        {
            auto parsed = smil::parse("<smil><head>" + std::string(head) + "</head><body>" + std::string(body) +
                                      "</body></smil>");
            REQUIRE(parsed.has_value());
            document = std::move(*parsed);
            media.load(document, base);
        }

        const smil::Media &at(std::size_t i) const { return std::get<smil::Media>(document.body.children.at(i)); }
    };
}

TEST_CASE("remote media: downloaded, then resolved to the cache file")
{
    Fixture f;
    f.serve("http://hub/media/a.jpg", "jpeg", "image/jpeg");
    f.load(R"(<img src="http://hub/media/a.jpg" dur="1s"/>)");

    CHECK_FALSE(f.media.resolve(f.at(0)).has_value()); // not there yet: the engine skips it
    f.runUntilIdle();
    auto path = f.media.resolve(f.at(0));
    REQUIRE(path.has_value());
    CHECK(f.fs.content(*path) == "jpeg");
    CHECK(path->starts_with("cache/"));
}

TEST_CASE("relative src against the base of the index, local and direct src as they are")
{
    Fixture f;
    f.serve("http://hub/index/img/b.jpg", "b", "image/jpeg");
    f.load(R"(<img src="img/b.jpg" dur="1s"/><img src="/usb/c.jpg" dur="1s"/><img src="data:image/png;base64,AA" dur="1s"/>
              <ref src="adapi:blankScreen" dur="1s"/>)");
    f.runUntilIdle();

    REQUIRE(f.media.resolve(f.at(0)).has_value());
    CHECK(f.fs.content(*f.media.resolve(f.at(0))) == "b");
    CHECK(f.media.resolve(f.at(1)) == "/usb/c.jpg");
    CHECK(f.media.resolve(f.at(2)) == "data:image/png;base64,AA");
    CHECK(f.media.resolve(f.at(3)) == "adapi:blankScreen");

    // a local index: relative to its directory
    Fixture local;
    local.load(R"(<img src="img/b.jpg" dur="1s"/>)", "/usb/show/");
    CHECK(local.media.resolve(local.at(0)) == "/usb/show/img/b.jpg");
}

TEST_CASE("not supported: not downloaded, MEDIA_UNSUPPORTED once per index version")
{
    ContentFixture f;
    smil::Document document;
    MediaManager   media(f.queue, f.inventory, f.fs, f.logger, "cache",
                         [](const smil::Media &m) { return m.kind != smil::MediaKind::Video; });
    f.serve("http://hub/v.mp4", "video", "video/mp4");

    auto parsed = smil::parse(R"(<smil><body><video src="http://hub/v.mp4"><param name="logContentId" value="42"/></video>
                                 <video src="http://hub/v.mp4"/></body></smil>)");
    REQUIRE(parsed.has_value());
    document = std::move(*parsed);
    media.load(document, "http://hub/");
    f.runUntilIdle();

    CHECK(f.requests("http://hub/v.mp4") == 0);
    CHECK_FALSE(media.resolve(std::get<smil::Media>(document.body.children[0])).has_value());
    std::string events = f.events();
    CHECK(events.find("<eventName>MEDIA_UNSUPPORTED</eventName>") != std::string::npos);
    CHECK(events.find("<meta name=\"contentId\" content=\"42\"/>") != std::string::npos);
    CHECK(events.find("<meta name=\"mediaType\" content=\"video\"/>") != std::string::npos);
    CHECK(std::ranges::count(events, '\n') == 1); // same src twice: once

    media.load(document, "http://hub/"); // a new index version reports again
    CHECK(std::ranges::count(f.events(), '\n') == 2);
}

TEST_CASE("web pages are not cacheable: the URL itself, unless onlyIfCached")
{
    Fixture f;
    f.serve("http://hub/page.html", "<html>", "text/html");
    f.load(R"(<text src="http://hub/page.html" dur="1s"/>
              <text src="http://hub/page.html" dur="1s"><param name="cacheControl" value="onlyIfCached"/></text>)");
    f.runUntilIdle();

    CHECK(f.media.resolve(f.at(0)) == "http://hub/page.html");
    CHECK_FALSE(f.media.resolve(f.at(1)).has_value());
}

TEST_CASE("an update replaces the file only when no instance plays it")
{
    Fixture f;
    f.serve("http://hub/a.jpg", "v1", "image/jpeg", "v1");
    f.load(R"(<img src="http://hub/a.jpg" dur="1s"/>)");
    f.runUntilIdle();
    const smil::Media &a = f.at(0);

    auto path = f.media.resolve(a); // an instance plays it
    REQUIRE(path.has_value());

    f.serve("http://hub/a.jpg", "v2", "image/jpeg", "v2");
    f.media.prefetch(a); // check for an update
    f.runUntilIdle();
    CHECK(f.fs.exists(*path + ".ready"));
    CHECK(f.fs.content(*path) == "v1"); // still playing: not replaced

    f.media.release(a);
    REQUIRE(f.media.resolve(a) == path);
    CHECK(f.fs.content(*path) == "v2");
    CHECK_FALSE(f.fs.exists(*path + ".ready"));
}

TEST_CASE("remote widget: extracted, resolved to its index.html, extracted again after an update")
{
    Fixture f;
    f.serve("http://hub/clock.wgt", makeZip({{"index.html", "v1"}}), "application/widget", "v1");
    f.load(R"(<ref src="http://hub/clock.wgt" type="application/widget" dur="1s"/>)");
    f.runUntilIdle();
    const smil::Media &widget = f.at(0);

    auto index = f.media.resolve(widget);
    REQUIRE(index.has_value());
    CHECK(index->ends_with("/index.html"));
    CHECK(f.fs.content(*index) == "v1");
    f.media.release(widget);

    f.clock.advance(1s);
    f.serve("http://hub/clock.wgt", makeZip({{"index.html", "v2"}}), "application/widget", "v2");
    f.media.prefetch(widget);
    f.runUntilIdle();
    REQUIRE(f.media.resolve(widget) == index);
    CHECK(f.fs.content(*index) == "v2");
}

TEST_CASE("region backgrounds are downloaded too, locate() finds them")
{
    Fixture f;
    f.serve("http://hub/bg.png", "png", "image/png");
    f.load("", "http://hub/", R"(<layout><root-layout width="100" height="100"/>
                                   <region xml:id="r" backgroundImage="http://hub/bg.png"/></layout>)");
    CHECK_FALSE(f.media.locate("http://hub/bg.png").has_value());
    f.runUntilIdle();
    auto path = f.media.locate("http://hub/bg.png");
    REQUIRE(path.has_value());
    CHECK(f.fs.content(*path) == "png");
}
