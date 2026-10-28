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
#ifndef GARLIC_CORE_PLAYER_RUNTIME_PLAYER_RUNTIME_API_HPP
#define GARLIC_CORE_PLAYER_RUNTIME_PLAYER_RUNTIME_API_HPP

#include "common/clock.hpp"
#include "common/file_system.hpp"
#include "configuration/configuration.hpp"
#include "content/cache_space.hpp"
#include "content/media_manager.hpp"
#include "downloader/http_transport.hpp"
#include "reports/reporter.hpp"
#include "smil/engine.hpp"
#include "tasks/task_scheduler.hpp"

#include <chrono>
#include <cstdint>
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace garlic::core
{
    namespace database
    {
        class InventoryTable;
    }
    namespace downloader
    {
        class DownloadQueue;
        struct DownloadResult;
    }
    namespace content
    {
        class IndexManager;
    }
    namespace httpd
    {
        class Router;
        class Server;
    }
    namespace rest_api
    {
        class RestApi;
    }
    namespace logger
    {
        class Logger;
    }

    /**
     * A new index: the frontend builds its regions anew from document->head (root_layout, regions sorted by z-index,
     * title). The frontend keeps the document until the next NewLayout: the StartMedia point into it.
     */
    struct NewLayout
    {
        std::shared_ptr<const smil::Document> document;
        /// backgroundImage of root-layout and regions as written in the SMIL → local path or URL; only those already
        /// there, the others follow as BackgroundImage
        std::map<std::string, std::string, std::less<>> backgrounds;
    };

    /// A background image of the current layout that was not there yet at NewLayout.
    struct BackgroundImage
    {
        std::string src;  // as written in the SMIL
        std::string path; // local path or URL
    };

    /// REST /v2/task/screenshot: the frontend saves a JPEG of the screen to path and calls screenshotTaken(path).
    struct Screenshot
    {
        std::string path;
    };

    /// Everything the frontend gets from poll(), in this order: e.g. StopMedia of the old index, NewLayout, StartMedia.
    using CoreEvent = std::variant<smil::StartMedia, smil::StopMedia, smil::RepeatMedia, smil::PauseMedia, smil::ResumeMedia,
                                   NewLayout, BackgroundImage, tasks::InstallSoftware, tasks::Reboot, tasks::ClearCaches,
                                   Screenshot>;

    struct PlayerRuntimeConfig
    {
        std::string ini_path;
        std::string cache_dir;
        std::string log_dir;
        std::string version;   // of the player software
        std::string index_uri; // from the command line; empty: the one in the ini
        std::string database_path; // SQLite file of the inventory; empty: <log_dir>/garlic.db like garlic-lib

        content::MediaManager::Supports     supports;    // what the frontend can show; empty: everything
        reports::Reporter::SystemInfoSource system_info; // empty: system::collectSystemInfo where it exists
        content::CacheSpace::FreeSpace      free_space;  // free bytes of the cache; empty: system::diskSpace where it
                                                         // exists, otherwise the cache is never cleaned up

        std::uint32_t random_seed        = 0; // for <seq> with pickNumber; pass a random value
        std::size_t   parallel_downloads = 2;
    };

    /// What the settings dialog of a frontend shows (garlic-lib: ConfigDialog); standby and restarts come from the CMS.
    struct PlayerSettings
    {
        std::string   player_name;
        std::string   index_uri;
        bool          httpd_enabled = true;
        std::uint16_t httpd_port    = 8080;
    };

    /**
     * The one interface of the core for the frontend (garlic-lib: LibFacade). It owns all modules, connects them and
     * is driven by poll() from one thread. The frontend only shows: it gets events and reports what only it knows.
     *
     *   PlayerRuntimeApi runtime(fs, clock, http, config);
     *   runtime.start();
     *   loop: for (const CoreEvent &event : runtime.poll()) std::visit(frontend, event);
     *         wait until runtime.nextDeadline() or input
     */
    class PlayerRuntimeApi
    {
        public:
            PlayerRuntimeApi(common::FileSystem &fs, const common::Clock &clock, downloader::HttpTransport &transport,
                             PlayerRuntimeConfig config);
            ~PlayerRuntimeApi();

            PlayerRuntimeApi(const PlayerRuntimeApi &)            = delete;
            PlayerRuntimeApi &operator=(const PlayerRuntimeApi &) = delete;

            /// Loads the configuration, opens the database, starts the HTTP server and the index.
            std::expected<void, std::string> start();

            /// Works off everything that is due and returns the events for the frontend.
            std::vector<CoreEvent> poll();

            /// When poll() is needed next; nullopt = only after input.
            std::optional<std::chrono::steady_clock::time_point> nextDeadline() const;

            // from the frontend
            void mediaFinished(smil::InstanceId id);
            void mediaFailed(smil::InstanceId id);
            void elementActivated(smil::InstanceId id);
            void accessKey(char key);
            void notify(std::string_view name);

            /// The screenshot of a Screenshot event is saved (or could not be): the REST API answers with it.
            void screenshotTaken(std::string_view path);

            configuration::Configuration &configuration() noexcept { return config; }

            PlayerSettings playerSettings() const;

            /**
             * Checks and stores the settings of the dialog: an invalid index URI or port gives the error text and
             * nothing is stored. The user agent follows the player name, a changed HTTP server is restarted (a port
             * in use keeps the old one running), the index is loaded again (garlic-lib: initParser after the dialog).
             */
            std::expected<void, std::string> changePlayerSettings(const PlayerSettings &next);

            /// Stops the playback, e.g. for standby or before quitting; the StopMedia come with the next poll().
            void stop();

            static constexpr std::chrono::milliseconds busy_poll{100};    // while downloads run or the HTTP server is on
            static constexpr std::chrono::seconds      resource_log{300}; // resources to debug.log, like garlic-lib

        private:
            common::FileSystem        &fs;
            const common::Clock       &clock;
            downloader::HttpTransport &transport;
            PlayerRuntimeConfig        settings;

            configuration::Configuration               config;
            std::unique_ptr<logger::Logger>            logger;
            std::unique_ptr<database::InventoryTable>  inventory;
            std::unique_ptr<content::CacheSpace>       cache_space; // only with free_space
            std::unique_ptr<downloader::DownloadQueue> queue;
            std::unique_ptr<content::IndexManager>     index;
            std::unique_ptr<content::MediaManager>     media;
            std::unique_ptr<reports::Reporter>         reporter;
            std::unique_ptr<tasks::TaskScheduler>      tasks;
            std::unique_ptr<httpd::Router>             router;
            std::unique_ptr<rest_api::RestApi>         rest_api;
            std::unique_ptr<httpd::Server>             server;

            std::shared_ptr<smil::Document> document; // shared with the frontend through NewLayout
            std::unique_ptr<smil::Engine>   engine;
            std::vector<CoreEvent>          events;
            std::set<std::string, std::less<>> missing_backgrounds; // of the current layout, not there yet

            std::optional<std::chrono::steady_clock::time_point> next_reboot;
            std::chrono::steady_clock::time_point                next_resource_log;

            void dispatch(const downloader::DownloadResult &result);
            void newIndex(smil::Document document, std::string base);
            void onTask(tasks::TaskAction action);
            void collectEngine();
            void findBackgrounds(std::map<std::string, std::string, std::less<>> *found);
            void planReboot(std::chrono::seconds not_before = {});
            void logResources();
            std::expected<void, std::string> restartServer(bool enabled, std::uint16_t port);
    };
}

#endif // GARLIC_CORE_PLAYER_RUNTIME_PLAYER_RUNTIME_API_HPP
