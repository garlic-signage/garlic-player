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

#include "common/std_file_system.hpp"
#include "httpd/mhd_server.hpp"

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
using namespace std::chrono_literals;

namespace
{
    /// Sends a raw HTTP request and returns the raw response (blocking, runs in its own thread).
    std::string sendRaw(std::uint16_t port, const std::string &request)
    {
        int         fd = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in address{};
        address.sin_family      = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port        = htons(port);
        if (::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
        {
            ::close(fd);
            return "connect failed";
        }

        std::size_t sent = 0;
        while (sent < request.size())
        {
            ssize_t n = ::send(fd, request.data() + sent, request.size() - sent, MSG_NOSIGNAL);
            if (n <= 0)
                break;
            sent += static_cast<std::size_t>(n);
        }

        std::string response;
        char        buffer[4096];
        ssize_t     n;
        while ((n = ::recv(fd, buffer, sizeof(buffer), 0)) > 0)
            response.append(buffer, static_cast<std::size_t>(n));
        ::close(fd);
        return response;
    }

    int statusOf(const std::string &response)
    {
        auto space = response.find(' ');
        return space == std::string::npos ? 0 : std::stoi(response.substr(space + 1, 3));
    }

    std::string bodyOf(const std::string &response)
    {
        auto end = response.find("\r\n\r\n");
        return end == std::string::npos ? "" : response.substr(end + 4);
    }

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
        std::string request(const std::string &raw)
        {
            auto client = std::async(std::launch::async, [port = server.port(), raw] { return sendRaw(port, raw); });
            auto end    = std::chrono::steady_clock::now() + 5s;
            while (client.wait_for(1ms) != std::future_status::ready && std::chrono::steady_clock::now() < end)
                server.poll();
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

