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

#include "httpd/router.hpp"

using namespace garlic::core::httpd;

namespace
{
    Request request(std::string method, std::string path)
    {
        Request r;
        r.method = std::move(method);
        r.path   = std::move(path);
        return r;
    }

    Handler answer(std::string text)
    {
        return [text](const Request &) { return Response::json(200, text); };
    }
}

TEST_CASE("Router calls the handler of method and path")
{
    Router router;
    router.add("POST", "/v2/oauth2/token", answer("token"));
    router.add("GET", "/v2/system/modelInfo", answer("model"));

    auto r1 = request("POST", "/v2/oauth2/token");
    auto r2 = request("GET", "/v2/system/modelInfo");
    CHECK(router.handle(r1).body == "token");
    CHECK(router.handle(r2).body == "model");
}

TEST_CASE("Router fills placeholders")
{
    Router      router;
    std::string id;
    router.add("GET", "/v2/files/{id}",
               [&](const Request &r)
               {
                   id = r.path_params.at("id");
                   return Response::json(200, "{}");
               });

    auto r = request("GET", "/v2/files/abc123");
    CHECK(router.handle(r).status == 200);
    CHECK(id == "abc123");
}

TEST_CASE("Router: fixed routes added first win over placeholders")
{
    Router router;
    router.add("GET", "/v2/files/find", answer("find"));
    router.add("GET", "/v2/files/{id}", answer("by id"));

    auto find = request("GET", "/v2/files/find");
    auto id   = request("GET", "/v2/files/xyz");
    CHECK(router.handle(find).body == "find");
    CHECK(router.handle(id).body == "by id");
}

TEST_CASE("Router: 404 for unknown paths, 405 for a known path with another method")
{
    Router router;
    router.add("GET", "/v2/files/{id}", answer("x"));

    auto unknown = request("GET", "/v2/unknown");
    auto deeper  = request("GET", "/v2/files/a/b");
    auto wrong   = request("DELETE", "/v2/files/a");
    CHECK(router.handle(unknown).status == 404);
    CHECK(router.handle(deeper).status == 404);
    CHECK(router.handle(wrong).status == 405);
}

TEST_CASE("Router ignores double and trailing slashes")
{
    Router router;
    router.add("GET", "/v2/system/firmwareInfo", answer("fw"));

    auto r = request("GET", "//v2/system/firmwareInfo/");
    CHECK(router.handle(r).body == "fw");
}

TEST_CASE("Request::param takes the URL parameter before the form field")
{
    Request r;
    r.query["access_token"] = "from-url";
    r.form["access_token"]  = "from-form";
    r.form["uri"]           = "http://x";

    CHECK(r.param("access_token") == "from-url");
    CHECK(r.param("uri") == "http://x");
    CHECK(r.param("missing").empty());
}

TEST_CASE("Request::header is case-insensitive")
{
    Request r;
    r.headers["content-type"] = "application/json";

    CHECK(r.header("Content-Type") == "application/json");
    CHECK(r.header("CONTENT-TYPE") == "application/json");
    CHECK(r.header("Accept").empty());
}
