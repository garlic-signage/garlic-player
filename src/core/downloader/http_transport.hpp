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
#ifndef GARLIC_CORE_DOWNLOADER_HTTP_TRANSPORT_HPP
#define GARLIC_CORE_DOWNLOADER_HTTP_TRANSPORT_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace garlic::core::downloader
{
    enum class HttpMethod
    {
        Head,
        Get
    };

    struct HttpHeader
    {
        std::string name;
        std::string value;
    };

    /// List of HTTP headers. Names are compared case-insensitive, as HTTP requires.
    class HttpHeaders
    {
        public:
            void add(std::string name, std::string value);

            /// Value of the first header with this name, or std::nullopt.
            std::optional<std::string_view> find(std::string_view name) const;

            const std::vector<HttpHeader> &all() const noexcept { return headers; }

        private:
            std::vector<HttpHeader> headers;
    };

    struct HttpRequest
    {
        HttpMethod  method = HttpMethod::Get;
        std::string url;
        HttpHeaders headers; // e.g. If-None-Match, If-Modified-Since, Range, User-Agent
    };

    struct HttpResponseHead
    {
        int         status = 0;
        HttpHeaders headers;
    };

    enum class TransportErrorCode
    {
        InvalidRequest,   // malformed URL, unsupported scheme …
        ConnectionFailed, // DNS, connect, connection reset
        TlsFailed,        // handshake, certificate
        Timeout,
        Cancelled,        // aborted by the handler (onData returned false)
        Busy,             // transport cannot take another request right now
        Other
    };

    struct TransportError
    {
        TransportErrorCode code;
        std::string        message; // for logging only
    };

    using RequestId = std::uint32_t;

    /**
     * Receives the response of one request.
     *
     * Order for a request: onHead() once, then onData() any number of times (never for HEAD), then onComplete() once.
     * On a transport error onHead() and onData() may be missing, onComplete() always comes last.
     * HTTP error status codes (404, 500 …) are no transport errors: they arrive in onHead() as usual.
     */
    class HttpResponseHandler
    {
        public:
            virtual ~HttpResponseHandler() = default;

            virtual void onHead(const HttpResponseHead &head) = 0;

            /// The chunk is only valid during the call. Return false to abort the request; onComplete() then follows
            /// with TransportErrorCode::Cancelled.
            virtual bool onData(std::span<const std::byte> chunk) = 0;

            virtual void onComplete(std::expected<void, TransportError> result) = 0;
    };

    /**
     * Executes HTTP requests. Implemented once per platform (libcurl, Emscripten Fetch, Tizen, webOS, ESP32) and as fake
     * for the tests.
     *
     * Rules every implementation follows:
     * - No own threads visible to the caller: handler callbacks are only invoked from inside poll(), never from start().
     *   This also allows a handler to start or cancel requests from within a callback.
     * - Redirects are not followed; 3xx responses go to the handler. Exception: platforms where the system follows
     *   redirects itself (browser Fetch) deliver the final response.
     * - A blocking library does a small piece of work per poll() call (e.g. read one chunk), so poll() stays short.
     * - Data is passed on in chunks and never buffered completely.
     */
    class HttpTransport
    {
        public:
            virtual ~HttpTransport() = default;

            /// Starts a request. The handler must stay valid until onComplete() was called or the request was cancelled.
            virtual std::expected<RequestId, TransportError> start(const HttpRequest &request,
                                                                   HttpResponseHandler &handler) = 0;

            /// Cancels a running request. After this no callback for this request is invoked anymore.
            /// Unknown or finished ids are ignored.
            virtual void cancel(RequestId id) = 0;

            /// Advances all running requests and invokes the handler callbacks.
            virtual void poll() = 0;
    };
}

#endif // GARLIC_CORE_DOWNLOADER_HTTP_TRANSPORT_HPP
