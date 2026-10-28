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
#ifndef GARLIC_CORE_HTTPD_REQUEST_HPP
#define GARLIC_CORE_HTTPD_REQUEST_HPP

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace garlic::core::httpd
{
    /// File of a multipart upload. The server stores it in a temporary file and removes it after the handler.
    struct UploadedFile
    {
        std::string   path;      // temporary file; the handler may rename it to keep it
        std::string   file_name; // as sent by the client
        std::string   content_type;
        std::uint64_t size = 0;
    };

    /// Request as the handlers see it, independent of the server library.
    struct Request
    {
        std::string                         method; // "GET", "POST", …
        std::string                         path;   // without query, e.g. "/v2/files/abc"
        std::map<std::string, std::string>  query;  // URL parameters
        std::map<std::string, std::string>  form;   // fields of url-encoded or multipart bodies
        std::map<std::string, UploadedFile> files;  // file fields of multipart bodies
        std::map<std::string, std::string>  headers;     // names in lowercase
        std::map<std::string, std::string>  path_params; // e.g. {id} of "/v2/files/{id}", set by the Router
        std::string                         body;        // raw body if it is no form, e.g. JSON

        /// URL parameter, or form field if there is none (like getParameter() of QtWebApp). Empty if missing.
        std::string param(std::string_view name) const;

        /// Header by name, case-insensitive. Empty if missing.
        std::string header(std::string_view name) const;
    };

    struct Response
    {
        int                                              status       = 200;
        std::string                                      content_type = "application/json";
        std::string                                      body;
        std::vector<std::pair<std::string, std::string>> headers;

        static Response json(int status, std::string body) { return {status, "application/json", std::move(body), {}}; }
    };

    using Handler = std::function<Response(const Request &)>;
}

#endif // GARLIC_CORE_HTTPD_REQUEST_HPP
