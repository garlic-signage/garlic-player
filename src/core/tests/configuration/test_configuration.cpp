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

#include "common/md5.hpp"
#include "common/memory_file_system.hpp"
#include "configuration/configuration.hpp"
#include "fake_clock.hpp"

#include <regex>

using namespace garlic::core::configuration;
using garlic::core::common::MemoryFileSystem;
using garlic::core::common::WriteMode;
using garlic::core::tests::FakeClock;
using namespace std::chrono_literals;

namespace
{
    constexpr std::string_view ini_path = "config/garlic-player.ini";

    struct Fixture
    {
        FakeClock        clock;
        MemoryFileSystem fs{clock};
        Configuration    config{fs, clock, std::string(ini_path)};

        void putIni(std::string_view text)
        {
            auto file = fs.openForWrite(ini_path, WriteMode::Truncate);
            REQUIRE(file.has_value());
            REQUIRE((*file)->write(std::as_bytes(std::span(text.data(), text.size()))).has_value());
            REQUIRE((*file)->close().has_value());
        }

        std::string ini() const { return fs.content(ini_path).value_or("<missing>"); }
    };
}

TEST_CASE("load reads an existing ini")
{
    Fixture f;
    f.putIni("[General]\nindex_uri=https://example.com/index.smil\nplayer_name=Lobby\n"
             "uuid=11111111-2222-4333-8444-555555555555\n\n[audio]\nvolume=80%\n");

    REQUIRE(f.config.load().has_value());

    CHECK(f.config.uuid() == "11111111-2222-4333-8444-555555555555");
    CHECK(f.config.playerName() == "Lobby");
    CHECK(f.config.indexUri() == "https://example.com/index.smil");
    CHECK(f.config.value("audio/volume") == "80%");
    CHECK(f.config.value("not/there").empty());
}

TEST_CASE("first start: uuid and player name are created and stored")
{
    Fixture f;
    REQUIRE(f.config.load().has_value());

    std::string uuid = f.config.uuid();
    CHECK(std::regex_match(uuid, std::regex("[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}")));
    CHECK(f.config.playerName() == uuid.substr(24)); // last part of the uuid, like garlic-lib
    CHECK(f.ini().find("uuid=" + uuid) != std::string::npos);
}

TEST_CASE("changes are written to the file immediately, without a temporary file left")
{
    Fixture f;
    REQUIRE(f.config.load().has_value());

    f.config.setPlayerName("Entrance");
    f.config.setValue("httpd/port", "9090");

    CHECK(f.ini().find("player_name=Entrance\n") != std::string::npos);
    CHECK(f.ini().find("[httpd]\nport=9090\n") != std::string::npos);
    CHECK_FALSE(f.fs.exists(std::string(ini_path) + ".tmp"));

    Configuration again{f.fs, f.clock, std::string(ini_path)};
    REQUIRE(again.load().has_value());
    CHECK(again.playerName() == "Entrance");
    CHECK(again.value("httpd/port") == "9090");
}

TEST_CASE("unknown keys of the ini are kept when saving")
{
    Fixture f;
    f.putIni("[General]\nuuid=u-1\nplayer_name=p\nfirmware_update_task_id=abc\n\n[httpd]\nminThreads=8\n");
    REQUIRE(f.config.load().has_value());

    f.config.setStandbyMode(std::string(standby_deep));

    CHECK(f.ini() == "[General]\nfirmware_update_task_id=abc\nplayer_name=p\nstandby_mode=deep\nuuid=u-1\n\n"
                     "[httpd]\nminThreads=8\n");
}

TEST_CASE("index URI from the command line wins and is stored")
{
    Fixture f;
    f.putIni("[General]\nindex_uri=https://old.example.com/index.smil\nuuid=u\nplayer_name=p\n");
    REQUIRE(f.config.load().has_value());

    f.config.determineIndexUri("");
    CHECK(f.config.indexUri() == "https://old.example.com/index.smil");

    f.config.determineIndexUri("https://new.example.com/index.smil");
    CHECK(f.config.indexUri() == "https://new.example.com/index.smil");
    CHECK(f.ini().find("index_uri=https://new.example.com/index.smil") != std::string::npos);
}

TEST_CASE("indexPath is the directory of the index")
{
    Fixture f;
    REQUIRE(f.config.load().has_value());

    auto pathFor = [&](std::string uri)
    {
        f.config.setIndexUri(std::move(uri));
        return f.config.indexPath();
    };

    CHECK(pathFor("https://example.com/smil/index.smil?player=1#x") == "https://example.com/smil/");
    CHECK(pathFor("https://garlic-hub.ddev.site/smil-index") == "https://garlic-hub.ddev.site/");
    CHECK(pathFor("https://example.com") == "https://example.com/");
    CHECK(pathFor("/var/lib/garlic/index.smil") == "/var/lib/garlic/");
    CHECK(pathFor("index.smil") == "./");
}

TEST_CASE("validateIndexUri")
{
    CHECK(Configuration::validateIndexUri("https://example.com/index.smil") == "https://example.com/index.smil");
    CHECK(Configuration::validateIndexUri("file:///var/index.smil") == "/var/index.smil");
    CHECK(Configuration::validateIndexUri("/var/index.smil") == "/var/index.smil");

    CHECK_FALSE(Configuration::validateIndexUri("").has_value());
    CHECK_FALSE(Configuration::validateIndexUri("http:/example.com").has_value());
    CHECK_FALSE(Configuration::validateIndexUri("index.smil").has_value());
}

TEST_CASE("api access token is valid for one day")
{
    Fixture f;
    REQUIRE(f.config.load().has_value());

    std::string token = f.config.determineApiAccessToken("admin", "secret");

    // MD5 of username + password + expire, uppercase like garlic-lib
    std::string expected = garlic::core::common::Md5::hex("adminsecret2026-01-02T00:00:00");
    for (char &c : expected)
        c = static_cast<char>(c >= 'a' && c <= 'f' ? c - 'a' + 'A' : c);
    CHECK(token == expected);
    CHECK(f.config.value("api_access_token_expire") == "2026-01-02T00:00:00");

    f.clock.advance(23h);
    CHECK(f.config.determineApiAccessToken("admin", "secret") == token);

    f.clock.advance(2h);
    CHECK(f.config.determineApiAccessToken("admin", "secret") != token);
}

TEST_CASE("user agent")
{
    Fixture f;
    f.putIni("[General]\nuuid=u-1\nplayer_name=Lobby\n");
    REQUIRE(f.config.load().has_value());
    f.config.setVersion("0.9.1");

    CHECK(f.config.userAgent() == "GAPI/1.0 (UUID:u-1; NAME:Lobby) garlic-" + std::string(Configuration::os()) +
                                      "/0.9.1 (MODEL:Garlic)");
}

TEST_CASE("paths given by the frontend")
{
    Fixture f;
    f.config.setCacheDir("/data/cache");
    f.config.setLogDir("/data/logs");

    CHECK(f.config.cacheDir() == "/data/cache");
    CHECK(f.config.logDir() == "/data/logs");
}

TEST_CASE("httpd settings from the ini, defaults for missing or invalid keys")
{
    Fixture f;
    f.putIni("[General]\nuuid=u\nplayer_name=p\n\n[httpd]\nhost=127.0.0.1\nmaxRequestSize=16000\n"
             "minThreads=8\nport=9090\nreadTimeout=1500\nmaxMultiPartSize=abc\n");
    REQUIRE(f.config.load().has_value());
    f.config.setCacheDir("/data/cache");

    auto s = f.config.httpd();
    CHECK(s.enabled);
    CHECK(s.bind_address == "127.0.0.1");
    CHECK(s.port == 9090);
    CHECK(s.max_request_size == 16000);
    CHECK(s.max_multipart_size == 114857600); // invalid: default
    CHECK(s.read_timeout == 2s);              // 1500 ms rounded up
    CHECK(s.upload_dir == "/data/cache");

    // defaults are not written into the ini
    CHECK(f.ini().find("maxMultiPartSize=abc") != std::string::npos);
}

TEST_CASE("httpd can be switched off in the ini")
{
    Fixture f;
    REQUIRE(f.config.load().has_value());
    CHECK(f.config.httpd().enabled);
    CHECK(f.config.httpd().port == 8080);

    f.config.setValue("httpd/enabled", "false");
    CHECK_FALSE(f.config.httpd().enabled);
}
