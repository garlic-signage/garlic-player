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

#include "common/memory_file_system.hpp"
#include "common/std_file_system.hpp"
#include "failing_file_system.hpp"
#include "fake_clock.hpp"
#include "httpd/mhd_server.hpp"
#include "raw_http.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <future>
#include <random>
#include <thread>

using namespace garlic::core::httpd;
using garlic::core::common::StdFileSystem;
using garlic::core::tests::bodyOf;
using garlic::core::tests::sendRaw;
using garlic::core::tests::statusOf;
using namespace std::chrono_literals;

namespace
{
    struct Fixture
    {
        StdFileSystem fs;
        Router        router;
        MhdServer     server{router, fs};
        std::string   upload_dir;

        Fixture()
        {
            std::random_device random;
            upload_dir = (std::filesystem::temp_directory_path() / ("garlic-httpd-" + std::to_string(random())))
                             .string();
            std::filesystem::create_directories(upload_dir);
        }

        ~Fixture()
        {
            server.stop();
            std::error_code ec;
            std::filesystem::remove_all(upload_dir, ec);
        }

        HttpdSettings settings() const
        {
            HttpdSettings s;
            s.port         = 0; // free port chosen by the system
            s.bind_address = "127.0.0.1";
            s.upload_dir   = upload_dir;
            return s;
        }

        void start(HttpdSettings s)
        {
            auto started = server.start(s);
            REQUIRE(started.has_value());
            REQUIRE(server.isRunning());
        }

        /// Sends the request from a client thread and polls the server here, like the core thread does.
        std::string request(const std::string &raw) { return request(server, raw); }

        std::string request(MhdServer &to, const std::string &raw)
        {
            auto client = std::async(std::launch::async, [port = to.port(), raw] { return sendRaw(port, raw); });
            auto end    = std::chrono::steady_clock::now() + 5s;
            while (client.wait_for(1ms) != std::future_status::ready && std::chrono::steady_clock::now() < end)
                to.poll();
            REQUIRE(client.wait_for(0s) == std::future_status::ready);
            return client.get();
        }
    };

    std::string post(const std::string &path, const std::string &content_type, const std::string &body)
    {
        return "POST " + path + " HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Type: " + content_type +
               "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    }
}

TEST_CASE("MhdServer: GET with URL parameters and headers reaches the handler")
{
    Fixture f;
    Request seen;
    f.router.add("GET", "/v2/system/modelInfo",
                 [&](const Request &r)
                 {
                     seen = r;
                     return Response::json(200, R"({"model":"garlic"})");
                 });
    f.start(f.settings());

    auto response = f.request("GET /v2/system/modelInfo?access_token=abc&x=1%202 HTTP/1.1\r\n"
                              "Host: localhost\r\nX-Test: Yes\r\nConnection: close\r\n\r\n");

    CHECK(statusOf(response) == 200);
    CHECK(bodyOf(response) == R"({"model":"garlic"})");
    CHECK(response.find("Content-Type: application/json") != std::string::npos);
    CHECK(seen.method == "GET");
    CHECK(seen.path == "/v2/system/modelInfo");
    CHECK(seen.param("access_token") == "abc");
    CHECK(seen.param("x") == "1 2");
    CHECK(seen.header("x-test") == "Yes");
}

TEST_CASE("MhdServer: unknown path gives 404")
{
    Fixture f;
    f.start(f.settings());

    auto response = f.request("GET /nothing HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    CHECK(statusOf(response) == 404);
}

TEST_CASE("MhdServer: url-encoded form fields")
{
    Fixture f;
    Request seen;
    f.router.add("POST", "/v2/oauth2/token",
                 [&](const Request &r)
                 {
                     seen = r;
                     return Response::json(200, "{}");
                 });
    f.start(f.settings());

    auto response = f.request(post("/v2/oauth2/token", "application/x-www-form-urlencoded",
                                   "grant_type=password&username=admin&password=s%26cret"));

    CHECK(statusOf(response) == 200);
    CHECK(seen.param("grant_type") == "password");
    CHECK(seen.param("username") == "admin");
    CHECK(seen.param("password") == "s&cret");
}

TEST_CASE("MhdServer: JSON body arrives unchanged")
{
    Fixture f;
    Request seen;
    f.router.add("POST", "/v2/app/exec",
                 [&](const Request &r)
                 {
                     seen = r;
                     return Response::json(200, "{}");
                 });
    f.start(f.settings());

    std::string json = R"({"uri":"http://example.com/index.smil"})";
    f.request(post("/v2/app/exec", "application/json", json));

    CHECK(seen.body == json);
    CHECK(seen.form.empty());
}

TEST_CASE("MhdServer: multipart upload is stored in a temporary file and removed afterwards")
{
    Fixture       f;
    Request       seen;
    std::string   uploaded_content;
    f.router.add("POST", "/v2/files/new",
                 [&](const Request &r)
                 {
                     seen             = r;
                     uploaded_content = f.fs.readFile(r.files.at("data").path).value_or("<missing>");
                     return Response::json(200, "{}");
                 });
    f.start(f.settings());

    std::string file_data(100'000, 'x'); // several pieces
    std::string body = "--BOUNDARY\r\n"
                       "Content-Disposition: form-data; name=\"downloadPath\"\r\n\r\n"
                       "/media/a.jpg\r\n"
                       "--BOUNDARY\r\n"
                       "Content-Disposition: form-data; name=\"data\"; filename=\"a.jpg\"\r\n"
                       "Content-Type: image/jpeg\r\n\r\n" +
                       file_data +
                       "\r\n"
                       "--BOUNDARY--\r\n";
    auto response = f.request(post("/v2/files/new", "multipart/form-data; boundary=BOUNDARY", body));

    CHECK(statusOf(response) == 200);
    CHECK(seen.param("downloadPath") == "/media/a.jpg");
    REQUIRE(seen.files.contains("data"));
    CHECK(seen.files.at("data").file_name == "a.jpg");
    CHECK(seen.files.at("data").content_type == "image/jpeg");
    CHECK(seen.files.at("data").size == file_data.size());
    CHECK(uploaded_content == file_data);
    CHECK_FALSE(f.fs.exists(seen.files.at("data").path)); // removed after the handler
}

TEST_CASE("MhdServer: the handler can keep an upload by renaming it")
{
    Fixture     f;
    std::string kept = f.upload_dir + "/kept.jpg";
    f.router.add("POST", "/v2/files/new",
                 [&](const Request &r)
                 {
                     (void)f.fs.rename(r.files.at("data").path, kept);
                     return Response::json(200, "{}");
                 });
    f.start(f.settings());

    std::string body = "--XyZ123\r\nContent-Disposition: form-data; name=\"data\"; filename=\"a.jpg\"\r\n\r\n"
                       "image\r\n--XyZ123--\r\n";
    f.request(post("/v2/files/new", "multipart/form-data; boundary=XyZ123", body));

    CHECK(f.fs.readFile(kept) == "image");
}

TEST_CASE("MhdServer: multipart without valid boundary gets 400")
{
    Fixture f;
    bool    called = false;
    f.router.add("POST", "/v2/files/new",
                 [&](const Request &)
                 {
                     called = true;
                     return Response::json(200, "{}");
                 });
    f.start(f.settings());

    auto response = f.request(post("/v2/files/new", "multipart/form-data", "--x\r\n\r\ndata\r\n--x--\r\n"));

    CHECK(statusOf(response) == 400);
    CHECK_FALSE(called);
}

TEST_CASE("MhdServer: too large bodies get 413 and never reach the handler")
{
    Fixture f;
    bool    called = false;
    f.router.add("POST", "/v2/app/exec",
                 [&](const Request &)
                 {
                     called = true;
                     return Response::json(200, "{}");
                 });

    auto s             = f.settings();
    s.max_request_size = 100;
    f.start(s);

    auto response = f.request(post("/v2/app/exec", "application/json", std::string(1000, 'x')));

    CHECK(statusOf(response) == 413);
    CHECK_FALSE(called);
}

TEST_CASE("MhdServer: disabled in the settings it does not listen")
{
    Fixture f;
    auto    s = f.settings();
    s.enabled = false;

    REQUIRE(f.server.start(s).has_value());
    CHECK_FALSE(f.server.isRunning());
    CHECK(f.server.port() == 0);
}

TEST_CASE("MhdServer: port in use is reported, stop() closes the port")
{
    Fixture f;
    f.start(f.settings());
    std::uint16_t port = f.server.port();
    CHECK(port != 0);

    StdFileSystem fs;
    Router        router;
    MhdServer     second{router, fs};
    auto          s = f.settings();
    s.port          = port;
    auto result     = second.start(s);
    CHECK_FALSE(result.has_value());

    f.server.stop();
    CHECK_FALSE(f.server.isRunning());
    CHECK(sendRaw(port, "GET / HTTP/1.1\r\n\r\n") == "connect failed");
}

TEST_CASE("MhdServer: invalid bind address")
{
    Fixture f;
    auto    s      = f.settings();
    s.bind_address = "not-an-address";

    CHECK_FALSE(f.server.start(s).has_value());
}

TEST_CASE("MhdServer: a later answer holds the connection without blocking, other requests go on")
{
    Fixture                          f;
    std::shared_ptr<PendingResponse> pending;
    f.router.add("GET", "/slow",
                 [&](const Request &)
                 {
                     pending = std::make_shared<PendingResponse>();
                     return Response::later(pending);
                 });
    f.router.add("GET", "/fast", [](const Request &) { return Response::json(200, R"({"fast":true})"); });
    f.start(f.settings());

    auto slow = std::async(std::launch::async, [port = f.server.port()]
                           { return sendRaw(port, "GET /slow HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"); });
    auto end = std::chrono::steady_clock::now() + 5s;
    while (!pending && std::chrono::steady_clock::now() < end)
        f.server.poll();
    REQUIRE(pending);

    // meanwhile the server answers other requests
    CHECK(bodyOf(f.request("GET /fast HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")) == R"({"fast":true})");
    CHECK(slow.wait_for(0s) != std::future_status::ready);

    pending->complete(Response{200, "image/jpeg", "JPEG", {}});
    while (slow.wait_for(1ms) != std::future_status::ready && std::chrono::steady_clock::now() < end)
        f.server.poll();
    REQUIRE(slow.wait_for(0s) == std::future_status::ready);
    auto response = slow.get();
    CHECK(statusOf(response) == 200);
    CHECK(response.find("Content-Type: image/jpeg") != std::string::npos);
    CHECK(bodyOf(response) == "JPEG");
}


TEST_CASE("MhdServer: an upload that cannot be stored gets 500 and never reaches the handler")
{
    Fixture f;
    bool    called = false;
    f.router.add("POST", "/v2/files/new",
                 [&](const Request &)
                 {
                     called = true;
                     return Response::json(200, "{}");
                 });
    auto s       = f.settings();
    s.upload_dir = f.upload_dir + "/missing";
    f.start(s);

    std::string body = "--XyZ123\r\nContent-Disposition: form-data; name=\"data\"; filename=\"a.jpg\"\r\n\r\nimage\r\n--XyZ123--\r\n";
    auto response = f.request(post("/v2/files/new", "multipart/form-data; boundary=XyZ123", body));

    CHECK(statusOf(response) == 500);
    CHECK_FALSE(called);
}

TEST_CASE("MhdServer: the headers of the response are sent")
{
    Fixture f;
    f.router.add("GET", "/file",
                 [](const Request &) { return Response{200, "image/jpeg", "JPEG", {{"Cache-Control", "no-cache"}}}; });
    f.start(f.settings());

    auto response = f.request("GET /file HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    CHECK(response.find("Cache-Control: no-cache\r\n") != std::string::npos);
}

TEST_CASE("MhdServer: destroying the server closes its port")
{
    Fixture       f;
    std::uint16_t port = 0;
    {
        MhdServer server{f.router, f.fs};
        REQUIRE(server.start(f.settings()).has_value());
        port = server.port();
    }
    CHECK(sendRaw(port, "GET / HTTP/1.1\r\n\r\n") == "connect failed");
}

TEST_CASE("MhdServer: a body of exactly the maximum size is accepted")
{
    Fixture     f;
    std::string seen;
    f.router.add("POST", "/v2/app/exec",
                 [&](const Request &r)
                 {
                     seen = r.body;
                     return Response::json(200, "{}");
                 });
    auto s             = f.settings();
    s.max_request_size = 100;
    f.start(s);

    auto response = f.request(post("/v2/app/exec", "application/json", std::string(100, 'x')));
    CHECK(statusOf(response) == 200);
    CHECK(seen.size() == 100);
}

TEST_CASE("MhdServer: a later answer that is already there is sent at once")
{
    Fixture f;
    f.router.add("GET", "/ready",
                 [](const Request &)
                 {
                     auto pending = std::make_shared<PendingResponse>();
                     pending->complete(Response::json(201, R"({"ready":true})"));
                     return Response::later(pending);
                 });
    f.start(f.settings());

    auto response = f.request("GET /ready HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    CHECK(statusOf(response) == 201);
    CHECK(bodyOf(response) == R"({"ready":true})");
}

TEST_CASE("MhdServer: stop() answers a waiting request with 503")
{
    Fixture                          f;
    std::shared_ptr<PendingResponse> pending;
    f.router.add("GET", "/slow",
                 [&](const Request &)
                 {
                     pending = std::make_shared<PendingResponse>();
                     return Response::later(pending);
                 });
    f.start(f.settings());

    auto slow = std::async(std::launch::async, [port = f.server.port()]
                           { return sendRaw(port, "GET /slow HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"); });
    auto end = std::chrono::steady_clock::now() + 5s;
    while (!pending && std::chrono::steady_clock::now() < end)
        f.server.poll();
    REQUIRE(pending);

    auto start = std::chrono::steady_clock::now();
    f.server.stop();
    CHECK(std::chrono::steady_clock::now() - start < 500ms); // the client closes after the 503: no waiting
    REQUIRE(slow.wait_for(5s) == std::future_status::ready);
    auto response = slow.get();
    CHECK(statusOf(response) == 503);
    CHECK(bodyOf(response) == R"({"error":"server stopped"})");

    f.start(f.settings()); // the closed connection is not resumed again
    CHECK(statusOf(f.request("GET /nothing HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")) == 404);
}

TEST_CASE("MhdServer: start() again closes the old port")
{
    Fixture f;
    f.start(f.settings());
    std::uint16_t first = f.server.port();

    f.start(f.settings());
    CHECK(f.server.port() != first);
    CHECK(sendRaw(first, "GET / HTTP/1.1\r\n\r\n") == "connect failed");
}

TEST_CASE("MhdServer: without a bind address on all addresses")
{
    Fixture f;
    f.router.add("GET", "/", [](const Request &) { return Response::json(200, "{}"); });
    auto s         = f.settings();
    s.bind_address = "";
    f.start(s);

    CHECK(statusOf(f.request("GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")) == 200);
}

TEST_CASE("MhdServer: a client that sends nothing more is disconnected after the read timeout")
{
    Fixture f;
    auto    s      = f.settings();
    s.read_timeout = 1s;
    f.start(s);

    auto start    = std::chrono::steady_clock::now();
    auto response = f.request("GET / HTTP/1.1\r\nHost: localhost\r\n"); // the empty line never comes
    CHECK(response.empty());
    CHECK(std::chrono::steady_clock::now() - start < 4s);
}

TEST_CASE("MhdServer: an idle client holds stop() up for 1 s at most")
{
    Fixture f;
    f.start(f.settings());

    int         fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port        = htons(f.server.port());
    REQUIRE(::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0);
    for (int i = 0; i < 10; ++i) // accepts the connection
        f.server.poll();

    auto start = std::chrono::steady_clock::now();
    f.server.stop();
    auto took = std::chrono::steady_clock::now() - start;
    ::close(fd);
    CHECK(took >= 900ms); // it waited for the client
    CHECK(took < 2s);
}

TEST_CASE("MhdServer: an upload that cannot be written or closed gets 500, its file is removed")
{
    for (bool close : {false, true})
    {
        CAPTURE(close);
        Fixture                                      f;
        garlic::core::tests::FakeClock               clock;
        garlic::core::common::MemoryFileSystem       memory{clock};
        garlic::core::tests::FailingFileSystem       failing{memory};
        MhdServer                                    server{f.router, failing};
        bool                                         called = false;
        f.router.add("POST", "/v2/files/new",
                     [&](const Request &)
                     {
                         called = true;
                         return Response::json(200, "{}");
                     });
        auto s       = f.settings();
        s.upload_dir = "uploads";
        REQUIRE(server.start(s).has_value());
        failing.fail_write = !close;
        failing.fail_close = close;

        std::string body = "--XyZ123\r\nContent-Disposition: form-data; name=\"data\"; filename=\"a.jpg\"\r\n\r\n"
                           "image\r\n--XyZ123--\r\n";
        auto response = f.request(server, post("/v2/files/new", "multipart/form-data; boundary=XyZ123", body));
        CHECK(statusOf(response) == 500);
        CHECK_FALSE(called);
        CHECK_FALSE(memory.list("uploads").has_value()); // nothing left
        server.stop();
    }
}
