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
#ifndef GARLIC_CORE_TESTS_FAKE_HTTP_TRANSPORT_HPP
#define GARLIC_CORE_TESTS_FAKE_HTTP_TRANSPORT_HPP

#include "downloader/http_transport.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace garlic::core::downloader::tests
{
    /**
     * HttpTransport without network for the tests.
     *
     * Responses are registered per method and URL. Each poll() advances every running request by one step
     * (head → one data chunk per call → complete), like a real asynchronous transport. Unregistered URLs answer 404.
     */
    class FakeHttpTransport : public HttpTransport
    {
        public:
            struct Response
            {
                int                           status = 200;
                HttpHeaders                   headers;
                std::string                   body;
                std::size_t                   chunk_size = 4;
                std::optional<TransportError> error; // set: the request fails instead of answering
            };

            void respond(HttpMethod method, std::string url, Response response)
            {
                responses[{method, std::move(url)}] = std::move(response);
            }

            std::expected<RequestId, TransportError> start(const HttpRequest &request,
                                                           HttpResponseHandler &handler) override
            {
                requests.push_back(request);

                Response response;
                response.status = 404;
                if (auto it = responses.find({request.method, request.url}); it != responses.end())
                    response = it->second;

                RequestId id = next_id++;
                active.emplace(id, Active{request.method, std::move(response), &handler});
                return id;
            }

            void cancel(RequestId id) override { active.erase(id); }

            void poll() override
            {
                std::vector<RequestId> ids;
                for (const auto &[id, a] : active)
                    ids.push_back(id);

                // Callbacks may start or cancel requests, so every step looks up its request again.
                for (RequestId id : ids)
                    step(id);
            }

            std::size_t activeCount() const noexcept { return active.size(); }

            /// All requests in the order they were started.
            std::vector<HttpRequest> requests;

        private:
            struct Active
            {
                HttpMethod           method;
                Response             response;
                HttpResponseHandler *handler;
                bool                 head_sent = false;
                std::size_t          offset    = 0;
            };

            void step(RequestId id)
            {
                auto it = active.find(id);
                if (it == active.end())
                    return;

                Active &a = it->second;
                if (a.response.error)
                {
                    finish(it, std::unexpected(*a.response.error));
                    return;
                }

                if (!a.head_sent)
                {
                    a.head_sent = true;
                    a.handler->onHead({a.response.status, a.response.headers});
                    return;
                }

                const std::string &body = a.response.body;
                if (a.method == HttpMethod::Get && a.offset < body.size())
                {
                    std::size_t          size  = std::min(a.response.chunk_size, body.size() - a.offset);
                    HttpResponseHandler *handler = a.handler;
                    auto chunk = std::as_bytes(std::span(body).subspan(a.offset, size));
                    a.offset += size;

                    if (!handler->onData(chunk))
                    {
                        if (auto again = active.find(id); again != active.end())
                            finish(again, std::unexpected(TransportError{TransportErrorCode::Cancelled, "aborted"}));
                    }
                    return;
                }

                finish(it, {});
            }

            void finish(std::map<RequestId, Active>::iterator it, std::expected<void, TransportError> result)
            {
                HttpResponseHandler *handler = it->second.handler;
                active.erase(it);
                handler->onComplete(std::move(result));
            }

            std::map<std::pair<HttpMethod, std::string>, Response> responses;
            std::map<RequestId, Active>                           active;
            RequestId                                             next_id = 1;
    };
}

#endif // GARLIC_CORE_TESTS_FAKE_HTTP_TRANSPORT_HPP
