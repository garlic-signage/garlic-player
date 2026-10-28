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
#ifndef GARLIC_CORE_HTTPD_ROUTER_HPP
#define GARLIC_CORE_HTTPD_ROUTER_HPP

#include "httpd/request.hpp"

#include <string>
#include <vector>

namespace garlic::core::httpd
{
    /**
     * Maps method and path to a handler. Patterns are paths with optional placeholders: "/v2/files/{id}".
     * The first matching route wins. Unknown path: 404, known path with another method: 405.
     */
    class Router
    {
        public:
            void add(std::string method, std::string pattern, Handler handler);

            /// Calls the matching handler; fills request.path_params.
            Response handle(Request &request) const;

        private:
            struct Route
            {
                std::string              method;
                std::vector<std::string> segments;
                Handler                  handler;
            };

            std::vector<Route> routes;
    };
}

#endif // GARLIC_CORE_HTTPD_ROUTER_HPP
