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
#include "fake_clock.hpp"
#include "rest_api/rest_api.hpp"

#include <nlohmann/json.hpp>

using namespace garlic::core;
using httpd::Request;
using httpd::Response;
using nlohmann::json;
using tests::FakeClock;
using namespace std::chrono_literals;

namespace
{
    struct Fixture
    {
        FakeClock                    clock;
        common::MemoryFileSystem     fs{clock};
        configuration::Configuration config{fs, clock, "garlic-player.ini"};
        database::InventoryTable     inventory{clock};
        rest_api::RestApi            api{config, inventory, fs, clock};
        httpd::Router                router;

        Fixture()
        {
            REQUIRE(config.load().has_value());
            config.setCacheDir("cache");
            REQUIRE(inventory.open(":memory:").has_value());
            api.registerRoutes(router);
        }

        Response call(std::string method, std::string path, std::map<std::string, std::string> query = {},
                      std::map<std::string, std::string> form = {}, std::string json_body = {})
        {
            Request r;
            r.method = std::move(method);
            r.path   = std::move(path);
            r.query  = std::move(query);
            r.form   = std::move(form);
            if (!json_body.empty())
            {
                r.headers["content-type"] = "application/json";
                r.body                    = std::move(json_body);
            }
            return router.handle(r);
        }

        /// Upload like the server delivers it: a temporary file in the cache directory.
        Response upload(const std::string &path, std::map<std::string, std::string> form, std::string_view content,
                        bool with_file = true)
        {
            Request r;
            r.method              = "POST";
            r.path                = path;
            r.form                = std::move(form);
            r.query["access_token"] = token();
            if (with_file)
            {
                putFile("cache/upload-1.tmp", content);
                r.files["data"] = {"cache/upload-1.tmp", "a.jpg", "image/jpeg", content.size()};
            }
            return router.handle(r);
        }

        void putFile(const std::string &path, std::string_view content)
        {
            auto file = fs.openForWrite(path, common::WriteMode::Truncate);
            REQUIRE(file.has_value());
            REQUIRE((*file)->write(std::as_bytes(std::span(content.data(), content.size()))).has_value());
            REQUIRE((*file)->close().has_value());
        }

        std::string token()
        {
            auto r = call("POST", "/v2/oauth2/token", {},
                          {{"grant_type", "password"}, {"username", "admin"}, {"password", ""}});
            return json::parse(r.body)["access_token"].get<std::string>();
        }

        std::map<std::string, std::string> auth() { return {{"access_token", token()}}; }
    };

    json body(const Response &r)
    {
        return json::parse(r.body);
    }

    std::string upper(std::string text)
    {
        for (char &c : text)
            c = static_cast<char>(c >= 'a' && c <= 'f' ? c - 'a' + 'A' : c);
        return text;
    }
}

// ------------------------------------------------------------------ oauth2

TEST_CASE("token: admin without password gets a token, like garlic-lib")
{
    Fixture f;
    auto    r = f.call("POST", "/v2/oauth2/token", {},
                       {{"grant_type", "password"}, {"username", "admin"}, {"password", ""}});

    CHECK(r.status == 200);
    CHECK(r.content_type == "application/json; charset=utf-8");
    auto b = body(r);
    CHECK(b["token_type"] == "Bearer");
    CHECK(b["expires_in"] == "2026-01-02T00:00:00");
    // garlic-lib hashed empty username and password: MD5(expire), uppercase
    CHECK(b["access_token"] == upper(common::Md5::hex("2026-01-02T00:00:00")));
}

TEST_CASE("token: JSON body and any method")
{
    Fixture f;
    auto    json_request = f.call("POST", "/v2/oauth2/token", {}, {},
                                  R"({"grant_type":"password","username":"admin","password":""})");
    auto    get          = f.call("GET", "/v2/oauth2/token",
                                  {{"grant_type", "password"}, {"username", "admin"}, {"password", ""}});

    CHECK(body(json_request).contains("access_token"));
    CHECK(body(get).contains("access_token"));
}

TEST_CASE("token: errors come with status 200 and the texts of garlic-lib")
{
    Fixture f;
    auto    error = [&](std::map<std::string, std::string> form)
    {
        auto r = f.call("POST", "/v2/oauth2/token", {}, std::move(form));
        CHECK(r.status == 200);
        return body(r)["error"].get<std::string>();
    };

    CHECK(error({{"grant_type", "client"}}) == "wrong grant_type");
    CHECK(error({{"grant_type", "password"}, {"username", "root"}}) == "wrong username");
    CHECK(error({{"grant_type", "password"}, {"username", "admin"}, {"password", "x"}}) == "wrong password");
    CHECK(body(f.call("POST", "/v2/oauth2/token", {}, {}, "not json"))["error"] == "No JSON found");
}

TEST_CASE("files without, with wrong or with expired token: 403")
{
    Fixture f;
    auto    no_token = f.call("GET", "/v2/files/find");
    CHECK(no_token.status == 403);
    CHECK(no_token.body == "{'error':'ACCESSVIOLATION'}");

    CHECK(f.call("GET", "/v2/files/find", {{"access_token", "WRONG"}}).status == 403);

    auto auth = f.auth();
    CHECK(f.call("GET", "/v2/files/find", auth).status == 200);
    f.clock.advance(25h);
    CHECK(f.call("GET", "/v2/files/find", auth).status == 403);
}

// ------------------------------------------------------------------ files/new

TEST_CASE("files/new stores the upload in the cache and in the inventory")
{
    Fixture     f;
    std::string download_path = "http://example.com/media/a.jpg";
    std::string id            = common::Md5::hex(download_path);

    auto r = f.upload("/v2/files/new", {{"downloadPath", download_path}, {"fileSize", "5"}}, "image");

    CHECK(r.status == 200);
    CHECK(r.headers == std::vector<std::pair<std::string, std::string>>{{"Cache-Control", "no-cache,no-store"},
                                                                       {"Pragma", "no-cache"}});
    auto b = body(r);
    CHECK(b["downloadPath"] == download_path);
    CHECK(b["fileSize"] == 5);
    CHECK(b["id"] == id);
    CHECK(b["transferredSize"] == 5);
    CHECK(b["completed"] == true);
    CHECK(b["createdDate"] == "2026-01-01T00:00:00");

    CHECK(f.fs.content("cache/" + id + ".jpg") == "image");
    CHECK_FALSE(f.fs.exists("cache/upload-1.tmp"));
    auto entry = f.inventory.find(download_path);
    REQUIRE(entry.has_value());
    CHECK(entry->file_name == id + ".jpg");
    CHECK(entry->content_type == "image/jpeg"); // from the extension, no mimeType sent
}

TEST_CASE("files/new: wrong size is not completed")
{
    Fixture f;
    auto    r = f.upload("/v2/files/new", {{"downloadPath", "http://x/a.png"}, {"fileSize", "100"}}, "short");

    CHECK(body(r)["completed"] == false);
    CHECK(body(r)["transferredSize"] == 5);
    CHECK(f.inventory.find("http://x/a.png")->state == downloader::InventoryState::Transfer);
}

TEST_CASE("files/new: missing fields")
{
    Fixture f;
    CHECK(body(f.upload("/v2/files/new", {{"downloadPath", "x"}, {"fileSize", "1"}}, "", false))["error"] ==
          "Data field missed");
    CHECK(body(f.upload("/v2/files/new", {{"downloadPath", "x"}}, "data"))["error"] == "fileSize field missed");
    CHECK(body(f.upload("/v2/files/new", {{"fileSize", "4"}}, "data"))["error"] == "downloadPath field missed");
}

// ------------------------------------------------------------------ files/find, files/{id}

TEST_CASE("files/find lists the inventory like garlic-lib")
{
    Fixture f;
    f.upload("/v2/files/new",
             {{"downloadPath", "http://x/a.jpg"}, {"fileSize", "5"}, {"modifiedDate", "2025-05-01T10:00:00"},
              {"mimeType", "image/x-test"}},
             "image");
    std::string id = common::Md5::hex("http://x/a.jpg");

    auto b = body(f.call("GET", "/v2/files/find", f.auth()));

    REQUIRE(b["items"].size() == 1);
    auto item = b["items"][0];
    CHECK(item["id"] == id);
    CHECK(item["etag"] == id);
    CHECK(item["mimeType"] == "image/x-test");
    CHECK(item["modifiedDate"] == "2025-05-01T10:00:00");
    CHECK(item["downloadPath"] == "cache/" + id + ".jpg"); // the local path, like garlic-lib
    CHECK(item["fileSize"] == "5");                        // a string, like garlic-lib
    CHECK(item["transferredSize"] == 5);
    CHECK(item["completed"] == true);
    CHECK(b["nextPageToken"] == 0);
}

TEST_CASE("files/find with paging, also as JSON body with strings or numbers")
{
    Fixture f;
    for (int i = 0; i < 3; ++i)
    {
        f.upload("/v2/files/new", {{"downloadPath", "http://x/" + std::to_string(i) + ".jpg"}, {"fileSize", "1"}},
                 "x");
        f.clock.advance(1s);
    }

    auto query = f.auth();
    query["maxResults"] = "2";
    query["pageToken"]  = "1";
    auto paged          = body(f.call("GET", "/v2/files/find", query));
    CHECK(paged["items"].size() == 2);
    CHECK(paged["nextPageToken"] == 3);

    auto as_strings = body(f.call("POST", "/v2/files/find", f.auth(), {}, R"({"maxResults":"1","pageToken":"0"})"));
    auto as_numbers = body(f.call("POST", "/v2/files/find", f.auth(), {}, R"({"maxResults":1,"pageToken":0})"));
    CHECK(as_strings["items"].size() == 1);
    CHECK(as_numbers["items"].size() == 1);
}

TEST_CASE("files/{id}: GET returns the entry or the error of garlic-lib")
{
    Fixture f;
    f.upload("/v2/files/new", {{"downloadPath", "http://x/a.jpg"}, {"fileSize", "5"}}, "image");
    std::string id = common::Md5::hex("http://x/a.jpg");

    CHECK(body(f.call("GET", "/v2/files/" + id, f.auth()))["id"] == id);
    CHECK(body(f.call("GET", "/v2/files/unknown", f.auth()))["error"] == "Record not found: unknown");
}

TEST_CASE("files/{id}: POST replaces the file of an existing entry")
{
    Fixture     f;
    std::string url = "http://x/a.jpg";
    f.upload("/v2/files/new", {{"downloadPath", url}, {"fileSize", "5"}}, "image");
    std::string id = common::Md5::hex(url);

    auto r = f.upload("/v2/files/" + id, {{"downloadPath", url}, {"fileSize", "7"}}, "changed");
    CHECK(body(r)["completed"] == true);
    CHECK(body(r)["id"] == id);
    CHECK(f.fs.content("cache/" + id + ".jpg") == "changed");

    CHECK(body(f.upload("/v2/files/" + id, {{"fileSize", "7"}}, "x"))["error"] == "downloadPath field missed");
    CHECK(body(f.upload("/v2/files/nothing", {{"downloadPath", url}}, "x"))["error"] == "Record not found: nothing");
    CHECK(body(f.upload("/v2/files/" + id, {{"downloadPath", "http://other"}}, "x"))["error"] ==
          "Record not identical to downloadPath" + id);
}

TEST_CASE("files/{id}: other methods are answered empty, like garlic-lib")
{
    Fixture f;
    auto    r = f.call("DELETE", "/v2/files/abc", f.auth());

    CHECK(r.status == 200);
    CHECK(r.body.empty());
}

// ------------------------------------------------------------------ files/delete

TEST_CASE("files/delete removes file and entry")
{
    Fixture     f;
    std::string url = "http://x/a.jpg";
    f.upload("/v2/files/new", {{"downloadPath", url}, {"fileSize", "5"}}, "image");
    std::string id = common::Md5::hex(url);

    auto query = f.auth();
    query["id"] = id;
    CHECK(body(f.call("POST", "/v2/files/delete", query))["success"] == id + " deleted");
    CHECK_FALSE(f.fs.exists("cache/" + id + ".jpg"));
    CHECK_FALSE(f.inventory.find(url).has_value());

    CHECK(body(f.call("POST", "/v2/files/delete", query))["error"] == "Record not found: " + id);
    CHECK(body(f.call("POST", "/v2/files/delete", f.auth(), {}, R"({"id":"x"})"))["error"] == "Record not found: x");
}

// ------------------------------------------------------------------ routing

TEST_CASE("unknown paths get the 404 text of garlic-lib")
{
    Fixture f;
    for (std::string path : {"/", "/v2", "/v2/files", "/v2/oauth2/other", "/v3/files/find", "/cache/a.jpg"})
    {
        CAPTURE(path);
        auto r = f.call("GET", path);
        CHECK(r.status == 404);
        CHECK(r.body == "The URL is wrong, no such document.");
        CHECK(r.content_type.empty());
    }
}
