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
#ifndef GARLIC_CORE_HTTPD_HTTPD_SETTINGS_HPP
#define GARLIC_CORE_HTTPD_HTTPD_SETTINGS_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace garlic::core::httpd
{
    /// Settings of the HTTP server, filled by configuration::Configuration::httpd() from garlic-player.ini.
    struct HttpdSettings
    {
        bool                 enabled = true;           // httpd/enabled
        std::string          bind_address;             // httpd/host, empty = all IPv4 interfaces
        std::uint16_t        port = 8080;              // httpd/port, 0 = chosen by the system
        std::chrono::seconds read_timeout{60};         // httpd/readTimeout (milliseconds in the ini)
        std::size_t          max_request_size   = 65536;     // httpd/maxRequestSize: body without file uploads
        std::size_t          max_multipart_size = 114857600; // httpd/maxMultiPartSize: multipart with files
        std::string          upload_dir;               // temporary files of uploads (the cache directory)
    };
}

#endif // GARLIC_CORE_HTTPD_HTTPD_SETTINGS_HPP
