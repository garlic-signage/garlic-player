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
#ifndef GARLIC_CORE_HTTPD_MHD_SERVER_HPP
#define GARLIC_CORE_HTTPD_MHD_SERVER_HPP

#include "common/file_system.hpp"
#include "httpd/router.hpp"
#include "httpd/server.hpp"

#include <memory>
#include <utility>
#include <vector>

struct MHD_Daemon;
struct MHD_Connection;

namespace garlic::core::httpd
{
    /// Server based on libmicrohttpd without its own threads (MHD_run in poll()). Linux, Windows, macOS, Android, iOS.
    class MhdServer final : public Server
    {
        public:
            MhdServer(const Router &router, common::FileSystem &fs);
            ~MhdServer() override;

            MhdServer(const MhdServer &)            = delete;
            MhdServer &operator=(const MhdServer &) = delete;

            std::expected<void, std::string> start(const HttpdSettings &settings) override;
            void                             stop() override;
            void                             poll() override;
            bool                             isRunning() const override { return daemon != nullptr; }
            std::uint16_t                    port() const override;

            struct Connection; // per request, only in the .cpp

        private:
            friend struct ConnectionAccess;

            const Router       &router;
            common::FileSystem &fs;
            HttpdSettings       settings;
            MHD_Daemon         *daemon       = nullptr;
            std::uint64_t       upload_count = 0;

            /// Connections held for a later answer (Response::later), resumed in poll() when it is complete.
            std::vector<std::pair<MHD_Connection *, std::shared_ptr<PendingResponse>>> suspended;
    };
}

#endif // GARLIC_CORE_HTTPD_MHD_SERVER_HPP
