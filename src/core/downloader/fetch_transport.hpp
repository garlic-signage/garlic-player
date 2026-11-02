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
#ifndef GARLIC_CORE_DOWNLOADER_FETCH_TRANSPORT_HPP
#define GARLIC_CORE_DOWNLOADER_FETCH_TRANSPORT_HPP

#include "downloader/http_transport.hpp"

#include <map>
#include <vector>

namespace garlic::core::downloader
{
    /// HttpTransport based on fetch() of the browser (WASM). The requests run in the event loop of the browser, poll()
    /// passes on what has arrived since the last call.
    class FetchTransport final : public HttpTransport
    {
        public:
            FetchTransport();
            ~FetchTransport() override;

            FetchTransport(const FetchTransport &)            = delete;
            FetchTransport &operator=(const FetchTransport &) = delete;

            std::expected<RequestId, TransportError> start(const HttpRequest &request,
                                                           HttpResponseHandler &handler) override;
            void cancel(RequestId id) override;
            void poll() override;

        private:
            struct Transfer
            {
                HttpResponseHandler *handler   = nullptr;
                bool                 head_sent = false;
            };

            void pollOne(RequestId id);
            void finish(RequestId id, std::expected<void, TransportError> result);

            std::map<RequestId, Transfer> transfers;
            std::vector<std::byte>        buffer; // one chunk is copied from JS into it
            RequestId                     next_id = 1;
    };
}

#endif // GARLIC_CORE_DOWNLOADER_FETCH_TRANSPORT_HPP
