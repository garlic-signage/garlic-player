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
#ifndef GARLIC_CORE_HTTPD_SERVER_HPP
#define GARLIC_CORE_HTTPD_SERVER_HPP

#include "httpd/httpd_settings.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace garlic::core::httpd
{
    /**
     * HTTP server, implemented per library (libmicrohttpd, later esp_http_server). Requests go to a Router.
     *
     * Like the rest of the core it starts no threads: the host calls poll() regularly, the handlers run inside it.
     */
    class Server
    {
        public:
            virtual ~Server() = default;

            /// Starts listening. Does nothing if settings.enabled is false.
            virtual std::expected<void, std::string> start(const HttpdSettings &settings) = 0;

            virtual void stop() = 0;

            /// Accepts connections, reads requests and calls the handlers. Returns immediately.
            virtual void poll() = 0;

            virtual bool isRunning() const = 0;

            /// Port the server listens on (useful with port 0), 0 if not running.
            virtual std::uint16_t port() const = 0;
    };
}

#endif // GARLIC_CORE_HTTPD_SERVER_HPP
