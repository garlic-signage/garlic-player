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
#ifndef GARLIC_CORE_REST_API_REST_API_HPP
#define GARLIC_CORE_REST_API_REST_API_HPP

#include "common/clock.hpp"
#include "common/file_system.hpp"
#include "configuration/configuration.hpp"
#include "database/inventory_table.hpp"
#include "httpd/router.hpp"

#include <nlohmann/json_fwd.hpp>

namespace garlic::core::rest_api
{
    /**
     * REST API v2 of the player (IAdea compatible), ported 1:1 from garlic-lib: same paths, parameters, JSON fields,
     * error texts and status codes, so existing clients keep working.
     *
     * Done: /v2/oauth2/token and /v2/files. Follow with their modules: /v2/system, /v2/task, /v2/app.
     */
    class RestApi
    {
        public:
            RestApi(configuration::Configuration &config, database::InventoryTable &inventory, common::FileSystem &fs,
                    const common::Clock &clock);

            /// Adds all routes and the 404 answer of garlic-lib to the router.
            void registerRoutes(httpd::Router &router);

            // --- /v2/oauth2
            httpd::Response token(const httpd::Request &request);

            // --- /v2/files
            httpd::Response filesNew(const httpd::Request &request);
            httpd::Response filesFind(const httpd::Request &request);
            httpd::Response filesDelete(const httpd::Request &request);
            httpd::Response fileById(const httpd::Request &request);

        private:
            bool        validToken(const httpd::Request &request) const;
            std::string cachePath(std::string_view cache_name) const;
            nlohmann::json fileInfo(std::string_view id) const;
            nlohmann::json storeUpload(const httpd::Request &request, std::string download_path,
                                       std::string cache_name, std::string_view id);

            configuration::Configuration &config;
            database::InventoryTable     &inventory;
            common::FileSystem           &fs;
            const common::Clock          &clock;
    };
}

#endif // GARLIC_CORE_REST_API_REST_API_HPP
