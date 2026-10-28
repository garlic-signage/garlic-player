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

#include "fake_http_transport.hpp"

#include <cstdint>
#include <string>
#include <vector>

using namespace garlic::core::downloader;
using garlic::core::downloader::tests::FakeHttpTransport;

namespace
{
    /// Records every callback as event string and collects the body.
    class RecordingHandler : public HttpResponseHandler
    {
        public:
            void onHead(const HttpResponseHead &head) override
            {
                events.push_back("head " + std::to_string(head.status));
                last_head = head;
            }

            bool onData(std::span<const std::byte> chunk) override
            {
                events.push_back("data");
                for (std::byte b : chunk)
                    body.push_back(static_cast<char>(b));
                return body.size() < abort_after;
            }

            void onComplete(std::expected<void, TransportError> result) override
            {
                if (result)
                    events.push_back("complete");
                else
                    events.push_back("error " + std::to_string(static_cast<int>(result.error().code)));
            }

            std::vector<std::string> events;
            std::string              body;
            HttpResponseHead         last_head;
            std::size_t              abort_after = SIZE_MAX;
    };

    void pollUntilIdle(FakeHttpTransport &transport)
    {
        for (int i = 0; i < 100 && transport.activeCount() > 0; ++i)
            transport.poll();
    }

    std::string errorEvent(TransportErrorCode code)
    {
        return "error " + std::to_string(static_cast<int>(code));
    }
}

TEST_CASE("HttpHeaders finds names case-insensitive")
{
    HttpHeaders headers;
    headers.add("Content-Type", "image/jpeg");
    headers.add("ETag", "\"abc\"");

    CHECK(headers.find("content-type") == "image/jpeg");
    CHECK(headers.find("ETAG") == "\"abc\"");
    CHECK_FALSE(headers.find("Last-Modified").has_value());
}

TEST_CASE("HttpHeaders returns the first of duplicate headers")
{
    HttpHeaders headers;
    headers.add("Set-Cookie", "a=1");
    headers.add("set-cookie", "b=2");

    CHECK(headers.find("Set-Cookie") == "a=1");
    CHECK(headers.all().size() == 2);
}

TEST_CASE("transport invokes no callback from start()")
{
    FakeHttpTransport transport;
    RecordingHandler  handler;

    auto id = transport.start({HttpMethod::Get, "http://example.com/a.jpg", {}}, handler);

    REQUIRE(id.has_value());
    CHECK(handler.events.empty());
}

TEST_CASE("GET delivers head, data in chunks and complete in this order")
{
    FakeHttpTransport           transport;
    FakeHttpTransport::Response response;
    response.headers.add("Content-Length", "10");
    response.body       = "0123456789";
    response.chunk_size = 4;
    transport.respond(HttpMethod::Get, "http://example.com/a.jpg", response);

    RecordingHandler handler;
    REQUIRE(transport.start({HttpMethod::Get, "http://example.com/a.jpg", {}}, handler));
    pollUntilIdle(transport);

    CHECK(handler.events == std::vector<std::string>{"head 200", "data", "data", "data", "complete"});
    CHECK(handler.body == "0123456789");
    CHECK(handler.last_head.headers.find("content-length") == "10");
}

TEST_CASE("HEAD delivers no data")
{
    FakeHttpTransport           transport;
    FakeHttpTransport::Response response;
    response.body = "ignored";
    transport.respond(HttpMethod::Head, "http://example.com/a.jpg", response);

    RecordingHandler handler;
    REQUIRE(transport.start({HttpMethod::Head, "http://example.com/a.jpg", {}}, handler));
    pollUntilIdle(transport);

    CHECK(handler.events == std::vector<std::string>{"head 200", "complete"});
}

TEST_CASE("HTTP error status is no transport error")
{
    FakeHttpTransport transport;
    RecordingHandler  handler;

    REQUIRE(transport.start({HttpMethod::Head, "http://example.com/unknown", {}}, handler));
    pollUntilIdle(transport);

    CHECK(handler.events == std::vector<std::string>{"head 404", "complete"});
}

TEST_CASE("redirect is passed to the handler and not followed")
{
    FakeHttpTransport           transport;
    FakeHttpTransport::Response response;
    response.status = 302;
    response.headers.add("Location", "http://cdn.example.com/a.jpg");
    transport.respond(HttpMethod::Head, "http://example.com/a.jpg", response);

    RecordingHandler handler;
    REQUIRE(transport.start({HttpMethod::Head, "http://example.com/a.jpg", {}}, handler));
    pollUntilIdle(transport);

    CHECK(handler.events == std::vector<std::string>{"head 302", "complete"});
    CHECK(handler.last_head.headers.find("location") == "http://cdn.example.com/a.jpg");
    CHECK(transport.requests.size() == 1);
}

TEST_CASE("transport error ends with onComplete")
{
    FakeHttpTransport           transport;
    FakeHttpTransport::Response response;
    response.error = TransportError{TransportErrorCode::ConnectionFailed, "connection refused"};
    transport.respond(HttpMethod::Get, "http://example.com/a.jpg", response);

    RecordingHandler handler;
    REQUIRE(transport.start({HttpMethod::Get, "http://example.com/a.jpg", {}}, handler));
    pollUntilIdle(transport);

    CHECK(handler.events == std::vector<std::string>{errorEvent(TransportErrorCode::ConnectionFailed)});
}

TEST_CASE("handler aborts by returning false from onData")
{
    FakeHttpTransport           transport;
    FakeHttpTransport::Response response;
    response.body       = "0123456789";
    response.chunk_size = 4;
    transport.respond(HttpMethod::Get, "http://example.com/a.jpg", response);

    RecordingHandler handler;
    handler.abort_after = 4;
    REQUIRE(transport.start({HttpMethod::Get, "http://example.com/a.jpg", {}}, handler));
    pollUntilIdle(transport);

    CHECK(handler.events == std::vector<std::string>{"head 200", "data", errorEvent(TransportErrorCode::Cancelled)});
    CHECK(handler.body == "0123");
}

TEST_CASE("cancel() stops all further callbacks")
{
    FakeHttpTransport           transport;
    FakeHttpTransport::Response response;
    response.body = "0123456789";
    transport.respond(HttpMethod::Get, "http://example.com/a.jpg", response);

    RecordingHandler handler;
    auto             id = transport.start({HttpMethod::Get, "http://example.com/a.jpg", {}}, handler);
    REQUIRE(id);

    transport.poll(); // head
    transport.cancel(*id);
    pollUntilIdle(transport);

    CHECK(handler.events == std::vector<std::string>{"head 200"});
    CHECK(transport.activeCount() == 0);
}

TEST_CASE("handler may start the next request from onComplete")
{
    FakeHttpTransport           transport;
    FakeHttpTransport::Response response;
    response.body = "data";
    transport.respond(HttpMethod::Head, "http://example.com/a.jpg", {});
    transport.respond(HttpMethod::Get, "http://example.com/a.jpg", response);

    // HEAD first, then GET from inside the callback, like the downloader will do it
    struct HeadThenGet : RecordingHandler
    {
        FakeHttpTransport *transport = nullptr;
        RecordingHandler   get_handler;

        void onComplete(std::expected<void, TransportError> result) override
        {
            RecordingHandler::onComplete(result);
            (void)transport->start({HttpMethod::Get, "http://example.com/a.jpg", {}}, get_handler);
        }
    } handler;
    handler.transport = &transport;

    REQUIRE(transport.start({HttpMethod::Head, "http://example.com/a.jpg", {}}, handler));
    pollUntilIdle(transport);

    CHECK(handler.events == std::vector<std::string>{"head 200", "complete"});
    CHECK(handler.get_handler.body == "data");
    CHECK(transport.requests.size() == 2);
}
