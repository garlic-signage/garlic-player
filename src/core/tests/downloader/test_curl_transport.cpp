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

#include "common/clock.hpp"
#include "common/memory_file_system.hpp"
#include "downloader/curl_transport.hpp"
#include "downloader/downloader.hpp"
#include "memory_inventory.hpp"
#include "test_http_server.hpp"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace garlic::core::downloader;
using garlic::core::downloader::tests::MemoryInventory;
using garlic::core::downloader::tests::TestHttpServer;
using namespace std::chrono_literals;

namespace
{
    class RecordingHandler : public HttpResponseHandler
    {
        public:
            void onHead(const HttpResponseHead &h) override
            {
                events.push_back("head " + std::to_string(h.status));
                head = h;
            }

            bool onData(std::span<const std::byte> chunk) override
            {
                ++chunks;
                for (std::byte b : chunk)
                    body.push_back(static_cast<char>(b));
                return !abort_on_data;
            }

            void onComplete(std::expected<void, TransportError> result) override
            {
                events.push_back(result ? "complete" : "error");
                if (!result)
                    error = result.error();
                done = true;
            }

            std::vector<std::string>      events;
            HttpResponseHead              head;
            std::string                   body;
            int                           chunks        = 0;
            bool                          abort_on_data = false;
            bool                          done          = false;
            std::optional<TransportError> error;
    };

    /// Polls until the handler is done, at most 5 seconds.
    void run(CurlTransport &transport, const RecordingHandler &handler)
    {
        auto end = std::chrono::steady_clock::now() + 5s;
        while (!handler.done && std::chrono::steady_clock::now() < end)
        {
            transport.poll();
            std::this_thread::sleep_for(1ms);
        }
        REQUIRE(handler.done);
    }

    /// A port where nobody listens.
    std::uint16_t unusedPort()
    {
        int         fd = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in address{};
        address.sin_family      = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ::bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address));
        socklen_t length = sizeof(address);
        ::getsockname(fd, reinterpret_cast<sockaddr *>(&address), &length);
        ::close(fd);
        return ntohs(address.sin_port);
    }
}

TEST_CASE("CurlTransport: GET delivers status, headers and the complete body")
{
    TestHttpServer server;
    std::string    body(200'000, 'x'); // several chunks
    server.route("GET", "/a.jpg", "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nETag: \"v1\"", body);

    CurlTransport    transport;
    RecordingHandler handler;
    REQUIRE(transport.start({HttpMethod::Get, server.url("/a.jpg"), {}}, handler));
    CHECK(handler.events.empty()); // nothing from start()

    run(transport, handler);

    CHECK(handler.events == std::vector<std::string>{"head 200", "complete"});
    CHECK(handler.head.headers.find("content-type") == "image/jpeg");
    CHECK(handler.head.headers.find("ETag") == "\"v1\"");
    CHECK(handler.body == body);
    CHECK(handler.chunks > 1);
}

TEST_CASE("CurlTransport: HEAD delivers no body")
{
    TestHttpServer server;
    server.route("HEAD", "/a.jpg", "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg", "data");

    CurlTransport    transport;
    RecordingHandler handler;
    REQUIRE(transport.start({HttpMethod::Head, server.url("/a.jpg"), {}}, handler));
    run(transport, handler);

    CHECK(handler.events == std::vector<std::string>{"head 200", "complete"});
    CHECK(handler.head.headers.find("Content-Length") == "4");
    CHECK(handler.body.empty());
}

TEST_CASE("CurlTransport: request headers are sent")
{
    TestHttpServer server;
    server.route("HEAD", "/a.jpg", "HTTP/1.1 304 Not Modified");

    CurlTransport transport;
    HttpRequest   request{HttpMethod::Head, server.url("/a.jpg"), {}};
    request.headers.add("User-Agent", "garlic-test");
    request.headers.add("If-None-Match", "\"v1\"");

    RecordingHandler handler;
    REQUIRE(transport.start(request, handler));
    run(transport, handler);

    CHECK(handler.events == std::vector<std::string>{"head 304", "complete"});
    REQUIRE(server.requests().size() == 1);
    CHECK(server.requests()[0].find("User-Agent: garlic-test") != std::string::npos);
    CHECK(server.requests()[0].find("If-None-Match: \"v1\"") != std::string::npos);
}

TEST_CASE("CurlTransport: HTTP errors arrive as status, not as transport error")
{
    TestHttpServer server;

    CurlTransport    transport;
    RecordingHandler handler;
    REQUIRE(transport.start({HttpMethod::Get, server.url("/missing"), {}}, handler));
    run(transport, handler);

    CHECK(handler.events == std::vector<std::string>{"head 404", "complete"});
}

TEST_CASE("CurlTransport: redirects are not followed")
{
    TestHttpServer server;
    server.route("HEAD", "/old", "HTTP/1.1 302 Found\r\nLocation: /new");

    CurlTransport    transport;
    RecordingHandler handler;
    REQUIRE(transport.start({HttpMethod::Head, server.url("/old"), {}}, handler));
    run(transport, handler);

    CHECK(handler.events == std::vector<std::string>{"head 302", "complete"});
    CHECK(handler.head.headers.find("Location") == "/new");
    CHECK(server.requests().size() == 1);
}

TEST_CASE("CurlTransport: connection refused is a transport error")
{
    CurlTransport    transport;
    RecordingHandler handler;
    REQUIRE(transport.start({HttpMethod::Get, "http://127.0.0.1:" + std::to_string(unusedPort()) + "/", {}}, handler));
    run(transport, handler);

    CHECK(handler.events == std::vector<std::string>{"error"});
    CHECK(handler.error->code == TransportErrorCode::ConnectionFailed);
}

TEST_CASE("CurlTransport: unsupported scheme is an invalid request")
{
    CurlTransport    transport;
    RecordingHandler handler;
    REQUIRE(transport.start({HttpMethod::Get, "foo://example.com/", {}}, handler));
    run(transport, handler);

    REQUIRE(handler.error.has_value());
    CHECK(handler.error->code == TransportErrorCode::InvalidRequest);
}

TEST_CASE("CurlTransport: handler aborts with false from onData")
{
    TestHttpServer server;
    server.route("GET", "/big", "HTTP/1.1 200 OK", std::string(200'000, 'x'));

    CurlTransport    transport;
    RecordingHandler handler;
    handler.abort_on_data = true;
    REQUIRE(transport.start({HttpMethod::Get, server.url("/big"), {}}, handler));
    run(transport, handler);

    CHECK(handler.events == std::vector<std::string>{"head 200", "error"});
    CHECK(handler.error->code == TransportErrorCode::Cancelled);
    CHECK(handler.chunks == 1);
}

TEST_CASE("CurlTransport: cancel() stops all callbacks")
{
    TestHttpServer server;
    server.route("GET", "/a.jpg", "HTTP/1.1 200 OK", "data");

    CurlTransport    transport;
    RecordingHandler handler;
    auto             id = transport.start({HttpMethod::Get, server.url("/a.jpg"), {}}, handler);
    REQUIRE(id);
    transport.cancel(*id);

    for (int i = 0; i < 50; ++i)
    {
        transport.poll();
        std::this_thread::sleep_for(1ms);
    }
    CHECK(handler.events.empty());
}

TEST_CASE("CurlTransport with the Downloader: real HTTP download into the cache")
{
    TestHttpServer server;
    server.route("HEAD", "/old.jpg", "HTTP/1.1 301 Moved Permanently\r\nLocation: /media/a.jpg");
    server.route("HEAD", "/media/a.jpg", "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nETag: \"v1\"", "image-data");
    server.route("GET", "/media/a.jpg", "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nETag: \"v1\"", "image-data");

    garlic::core::common::SystemClock      clock;
    garlic::core::common::MemoryFileSystem fs{clock};
    MemoryInventory                        inventory;
    CurlTransport                          transport;

    DownloaderConfig config;
    config.cache_dir = "cache";
    Downloader downloader{transport, fs, inventory, clock, config};

    std::optional<DownloadResult> result;
    REQUIRE(downloader.start(server.url("/old.jpg"), {}, [&](const DownloadResult &r) { result = r; }));

    auto end = std::chrono::steady_clock::now() + 5s;
    while (!result && std::chrono::steady_clock::now() < end)
    {
        transport.poll();
        downloader.poll();
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(result.has_value());
    CHECK(result->status == DownloadStatus::Downloaded);
    CHECK(fs.content(result->local_path) == "image-data");
    CHECK(server.requests().size() == 3); // HEAD, HEAD after redirect, GET
}
