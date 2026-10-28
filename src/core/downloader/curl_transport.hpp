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
#ifndef GARLIC_CORE_DOWNLOADER_CURL_TRANSPORT_HPP
#define GARLIC_CORE_DOWNLOADER_CURL_TRANSPORT_HPP

#include "downloader/http_transport.hpp"

#include <chrono>
#include <map>
#include <memory>
#include <string>

namespace garlic::core::downloader
{
    struct CurlTransportConfig
    {
        std::chrono::seconds connect_timeout{30};

        /// CA certificates (PEM). Empty: the default of libcurl/OpenSSL, which works on Linux. Android, iOS, macOS and
        /// Windows have no OpenSSL certificate store, there a bundle has to be passed.
        std::string ca_bundle;
    };

    /// HttpTransport based on libcurl (curl_multi, no threads). Linux, Windows, macOS, Android, iOS.
    class CurlTransport final : public HttpTransport
    {
        public:
            explicit CurlTransport(CurlTransportConfig config = {});
            ~CurlTransport() override;

            CurlTransport(const CurlTransport &)            = delete;
            CurlTransport &operator=(const CurlTransport &) = delete;

            std::expected<RequestId, TransportError> start(const HttpRequest &request,
                                                           HttpResponseHandler &handler) override;
            void cancel(RequestId id) override;
            void poll() override;

            struct Transfer; // libcurl details, only in the .cpp

        private:
            void remove(std::map<RequestId, std::unique_ptr<Transfer>>::iterator it);

            CurlTransportConfig                          config;
            void                                        *multi; // CURLM, kept out of the header
            std::map<RequestId, std::unique_ptr<Transfer>> transfers;
            RequestId                                    next_id    = 1;
            bool                                         in_perform = false;
    };
}

#endif // GARLIC_CORE_DOWNLOADER_CURL_TRANSPORT_HPP
