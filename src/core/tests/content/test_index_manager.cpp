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
#include "content/index_manager.hpp"
#include "content_fixture.hpp"

using namespace garlic::core;
using namespace garlic::core::content;
using namespace garlic::core::tests;
using namespace std::chrono_literals;

namespace
{
    std::string indexText(std::string_view title, int refresh = 0)
    {
        return R"(<smil><head><meta name="title" content=")" + std::string(title) +
               R"("/><meta http-equiv="Refresh" content=")" + std::to_string(refresh) +
               R"("/></head><body/></smil>)";
    }

    struct Fixture : ContentFixture
    {
        configuration::Configuration config{fs, clock, "config/garlic-player.ini"};
        std::vector<std::string>     titles; // of every index handed on
        std::vector<std::string>     bases;
        IndexManager                 index{queue, fs, clock, logger, config, "cache",
                               [this](smil::Document d, std::string base)
                               {
                                   titles.push_back(d.head.title);
                                   bases.push_back(std::move(base));
                               }};

        explicit Fixture(std::string uri = "http://hub/player/index.smil?id=1")
        {
            REQUIRE(config.load().has_value());
            config.setIndexUri(std::move(uri));
            on_result = [this](const downloader::DownloadResult &r) { index.downloaded(r); };
        }

        /// Lets time pass and polls like the host.
        void wait(std::chrono::seconds time)
        {
            clock.advance(time);
            index.poll();
            runUntilIdle();
        }
    };
}

TEST_CASE("first start: download, hand on, remember as last played")
{
    Fixture f;
    f.serve("http://hub/player/index.smil?id=1", indexText("first"), "application/smil+xml");
    f.index.start();
    f.runUntilIdle();

    CHECK(f.titles == std::vector<std::string>{"first"});
    CHECK(f.bases == std::vector<std::string>{"http://hub/player/"});
    CHECK(f.config.lastPlayedIndexPath() == "cache/index.smil");
    CHECK(f.events().find("<eventName>SMIL_LOADED</eventName>") != std::string::npos);
}

TEST_CASE("the last played index plays at once, also offline")
{
    Fixture f;
    writeFile(f.fs, "cache/index.smil", indexText("cached"));
    f.config.setLastPlayedIndexPath("cache/index.smil");
    downloader::tests::FakeHttpTransport::Response offline;
    offline.error = downloader::TransportError{downloader::TransportErrorCode::ConnectionFailed, "offline"};
    f.transport.respond(downloader::HttpMethod::Head, "http://hub/player/index.smil?id=1", offline);

    f.index.start();
    CHECK(f.titles == std::vector<std::string>{"cached"}); // before any download
    f.runUntilIdle();
    CHECK(f.titles == std::vector<std::string>{"cached"});
}

TEST_CASE("refresh: at least 60 s, an update is handed on, not modified is not")
{
    Fixture f;
    f.serve("http://hub/player/index.smil?id=1", indexText("v1", 10), "application/smil+xml", "v1");
    f.index.start();
    f.runUntilIdle();
    REQUIRE(f.titles.size() == 1);

    f.wait(30s);
    CHECK(f.requests("http://hub/player/index.smil?id=1") == 2); // HEAD + GET of the first download only
    f.wait(30s);                                                 // 60 s: the minimum
    CHECK(f.requests("http://hub/player/index.smil?id=1") > 2);
    CHECK(f.titles.size() == 1); // not modified

    f.serve("http://hub/player/index.smil?id=1", indexText("v2", 10), "application/smil+xml", "v2");
    f.wait(60s);
    CHECK(f.titles == std::vector<std::string>{"v1", "v2"});
    CHECK(f.fs.content("cache/index.smil")->find("v2") != std::string::npos);
}

TEST_CASE("a broken update is deleted, the current index plays on")
{
    Fixture f;
    f.serve("http://hub/player/index.smil?id=1", indexText("good", 60), "application/smil+xml", "v1");
    f.index.start();
    f.runUntilIdle();

    f.serve("http://hub/player/index.smil?id=1", "<smil><head>", "application/smil+xml", "v2");
    f.wait(60s);
    CHECK(f.titles == std::vector<std::string>{"good"});
    CHECK(f.fs.content("cache/index.smil")->find("good") != std::string::npos);
    CHECK_FALSE(f.fs.exists("cache/index.smil.ready"));
    CHECK(f.events().find("<eventName>SMIL_OPEN_ERROR</eventName>") != std::string::npos);
}

TEST_CASE("without a valid index: try again every 60 s (garlic-lib restarted the player)")
{
    Fixture f;
    f.serve("http://hub/player/index.smil?id=1", "broken", "application/smil+xml", "v1");
    f.index.start();
    f.runUntilIdle();
    CHECK(f.titles.empty());
    REQUIRE(f.index.nextDeadline().has_value());

    f.serve("http://hub/player/index.smil?id=1", indexText("fixed"), "application/smil+xml", "v2");
    f.wait(60s);
    CHECK(f.titles == std::vector<std::string>{"fixed"});
}

TEST_CASE("refresh 0: no check any more")
{
    Fixture f;
    f.serve("http://hub/player/index.smil?id=1", indexText("once", 0), "application/smil+xml");
    f.index.start();
    f.runUntilIdle();
    CHECK_FALSE(f.index.nextDeadline().has_value());
}

TEST_CASE("local index: read directly, base is its directory")
{
    Fixture f("/usb/show/index.smil");
    writeFile(f.fs, "/usb/show/index.smil", indexText("usb"));
    f.index.start();
    CHECK(f.titles == std::vector<std::string>{"usb"});
    CHECK(f.bases == std::vector<std::string>{"/usb/show/"});
    CHECK(f.index.url().empty());
    CHECK(f.transport.requests.empty());
}
