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

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace garlic::core::rest_api
{
    /// What the REST API cannot do itself; given by the PlayerRuntimeApi. Empty functions do nothing.
    struct RestActions
    {
        std::function<void(std::string_view name)> notify;        // /v2/task/notify: notify(…) triggers
        std::function<void(std::string task_id)>   reboot;        // /v2/task/reboot
        std::function<void()>                      reloadIndex;   // /v2/app/start, /v2/app/switch
        std::function<void(std::string uri)>       playTemporary; // /v2/app/exec
        std::function<void(std::string path)>      screenshot;    // /v2/task/screenshot: the frontend saves a JPEG
    };

    /**
     * REST API v2 of the player (IAdea compatible), ported 1:1 from garlic-lib: same paths, parameters, JSON fields,
     * error texts and status codes, so existing clients keep working.
     *
     * /v2/oauth2/token, /v2/files, /v2/system, /v2/app, /v2/task.
     */
    class RestApi
    {
        public:
            RestApi(configuration::Configuration &config, database::InventoryTable &inventory, common::FileSystem &fs,
                    const common::Clock &clock, RestActions actions = {});

            /// Adds all routes and the 404 answer of garlic-lib to the router.
            void registerRoutes(httpd::Router &router);

            // --- /v2/oauth2
            httpd::Response token(const httpd::Request &request);

            // --- /v2/files
            httpd::Response filesNew(const httpd::Request &request);
            httpd::Response filesFind(const httpd::Request &request);
            httpd::Response filesDelete(const httpd::Request &request);
            httpd::Response fileById(const httpd::Request &request);

            // --- /v2/system, /v2/app, /v2/task
            httpd::Response system(const httpd::Request &request);
            httpd::Response app(const httpd::Request &request);
            httpd::Response task(const httpd::Request &request);

            /// The frontend has saved the screenshot (or tried): the held answer is sent.
            void screenshotTaken(std::string_view path);

            /// Answers screenshots that did not come in time with "file not created." like garlic-lib.
            void poll();
            std::optional<std::chrono::steady_clock::time_point> nextDeadline() const;

            static constexpr std::chrono::seconds screenshot_timeout{5};

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
            RestActions                   actions;

            struct Screenshot
            {
                std::string                             path;
                std::shared_ptr<httpd::PendingResponse> pending;
                std::chrono::steady_clock::time_point   deadline;
            };
            std::vector<Screenshot> screenshots;

            httpd::Response appStart(const httpd::Request &request, bool temporary);
            httpd::Response screenshot();
            httpd::Response screenshotFile(const std::string &path) const;
    };
}

#endif // GARLIC_CORE_REST_API_REST_API_HPP
