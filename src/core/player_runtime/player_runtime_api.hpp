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
#include "player_runtime/supervisor.hpp"
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

    /**
     * Master volume and brightness of the configuration (audio/volume, display/brightness), 0…100; nullopt: not set
     * or no number. At the start and whenever a configuration changes them. The player applies neither, the frontend
     * passes them to the supervisor (setVolume, setBrightness).
     */
    struct DeviceSettings
    {
        std::optional<int> volume;
        std::optional<int> brightness;

        bool operator==(const DeviceSettings &) const = default;
    };

    /**
     * The downloads of the media (not the index or task files), whenever they change: done of total files since no
     * medium was downloading, and the bytes of the running media download that started first (bytes_total 0: size
     * unknown), at most every percent; {0, 0} when no medium is downloading any more. The frontend shows "Loading done+1 / total" with a bar of
     * these bytes until the first medium of a layout, so a screen is not black and the bar moves at once.
     */
    struct Downloads
    {
        std::size_t   done        = 0;
        std::size_t   total       = 0;
        std::uint64_t bytes_done  = 0;
        std::uint64_t bytes_total = 0;

        bool operator==(const Downloads &) const = default;
    };

    /// Everything the frontend gets from poll(), in this order: e.g. StopMedia of the old index, NewLayout, StartMedia.
    using CoreEvent = std::variant<smil::StartMedia, smil::StopMedia, smil::RepeatMedia, smil::PauseMedia, smil::ResumeMedia,
                                   NewLayout, BackgroundImage, tasks::ClearCaches, Screenshot, DeviceSettings, Downloads>;

    struct PlayerRuntimeConfig
    {
        std::string ini_path;
        std::string cache_dir;
        std::string log_dir;
        std::string version;   // of the player software
        std::string index_uri; // from the command line; empty: the one in the ini
        std::string database_path; // SQLite file of the inventory; empty: <log_dir>/garlic.db like garlic-lib
        std::string configuration_file; // -c config.xml, e.g. of the launcher: applied at every start; empty: none
        std::string uuid; // of the supervisor (launcher, display): taken only if the ini has none yet; empty: a new one
        std::string launcher; // "philips-FB01.09": name and version of the launcher, only in the user agent; empty: none

        content::MediaManager::Supports     supports;    // what the frontend can show; empty: everything
        reports::Reporter::SystemInfoSource system_info; // empty: system::collectSystemInfo where it exists
        content::CacheSpace::FreeSpace      free_space;  // free bytes of the cache; empty: system::diskSpace where it
                                                         // exists, otherwise the cache is never cleaned up
        Supervisor *supervisor = nullptr; // restarts and installations; must outlive the runtime. nullptr: none, they
                                          // are only written to debug.log

        std::uint32_t random_seed        = 0; // for <seq> with pickNumber; pass a random value
        std::size_t   parallel_downloads = 2;
        bool          preview            = false; // preview in garlic-hub: only index and media are loaded, nothing
                                                  // is sent (no reports) and no tasks are fetched
    };

    /// What the settings dialog of a frontend shows (garlic-lib: ConfigDialog); standby and restarts come from the CMS.
    struct PlayerSettings
    {
        std::string   player_name;
        std::string   index_uri;
        bool          httpd_enabled = true;
        std::uint16_t httpd_port    = 8080;
    };

    /// What the debug dialog of a frontend shows (garlic-lib: DebugInfos, ResourceMonitor). Empty where unknown.
    struct DebugInfo
    {
        std::string                  index_title;
        std::string                  user_agent;
        std::string                  version; // of the player, the file VERSION (common::version())
        std::optional<std::uint64_t> disk_total;   // of the cache, 96 % of the disk like garlic-lib
        std::optional<std::uint64_t> disk_free;
        std::optional<std::uint64_t> memory_total; // of the system
        std::optional<std::uint64_t> memory_free;
        std::uint64_t                memory_used = 0; // of the player (resident set size)
        std::uint64_t                memory_max  = 0; // since the start
        std::string                  memory_max_time; // ISO 8601, when the maximum was measured
        int                          threads     = 0;
        int                          threads_max = 0;
        /// Load of the whole CPU (all cores = 100) by the player with the processes it started (CEF), since the
        /// previous debugInfo(); empty at the first call and where the platform cannot tell (WASM)
        std::optional<int> cpu;
        std::optional<int> cpu_max;      // the highest average over the 5 minutes of the resource log, since the start
        std::string        cpu_max_time; // ISO 8601, the end of those 5 minutes
        /// Load of the whole system (all cores = 100), same interval as cpu; empty where the platform cannot tell
        /// (Android, WASM)
        std::optional<int> cpu_system;
        /// °C, the highest sensor of each part (Linux, Android), and whether the device throttles now (Linux on a
        /// Raspberry Pi or Intel, Android 11+, Windows, Apple); empty where the platform cannot tell
        std::optional<int>  temperature_cpu;
        std::optional<int>  temperature_ram;
        std::optional<int>  temperature_ssd;
        std::optional<bool> throttling;
        /// Since the start of the player and since the boot of the device; a player clearly younger than the device
        /// was started again (crash, watchdog, supervisor). The time in sleep counts in both where the device is
        /// known; WASM: only the player, as the time of the page.
        std::chrono::seconds                uptime_player{};
        std::optional<std::chrono::seconds> uptime_system;

        struct Download
        {
            std::string   name;     // title of the medium in the SMIL, otherwise the file name of the URL
            std::uint64_t received = 0;
            std::uint64_t total    = 0; // 0: size not known yet
        };
        std::vector<Download> downloads;         // running, in the order they started
        std::size_t           downloads_waiting = 0;
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

            /// The frontend has handled a ClearCaches event (web cache cleared, if asked): now the core restarts.
            void cachesCleared();

            /// A warning of the frontend into debug.log, e.g. the watchdog of the Android player; also before start().
            void logWarning(std::string_view category, std::string_view message);

            configuration::Configuration &configuration() noexcept { return config; }

            PlayerSettings playerSettings() const;

            /**
             * Checks and stores the settings of the dialog: an invalid index URI or port gives the error text and
             * nothing is stored. The user agent follows the player name, a changed HTTP server is restarted (a port
             * in use keeps the old one running), the index is loaded again (garlic-lib: initParser after the dialog).
             */
            std::expected<void, std::string> changePlayerSettings(const PlayerSettings &next);

            /// Resources and identity for the debug dialog; also updates the maxima.
            DebugInfo debugInfo();

            /**
             * Applies a local configuration.xml now, e.g. one the launcher sends (garlic-lib: setConfigFromExternal):
             * like the task updateSettings, the index is loaded again. Missing or broken: an event, false.
             */
            bool applyConfigurationFile(std::string_view path);

            /// Time until the playing instance ends by its own dur or end (Engine::plannedEnd), rounded up; nullopt:
            /// nothing planned. For the deep standby of adapi:blankScreen (garlic-lib: determineNextEndTime).
            std::optional<std::chrono::seconds> remainingTime(smil::InstanceId id) const;

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
            std::optional<std::string>                           reboot_after_clear; // task id until cachesCleared()
            DeviceSettings                                       device_settings;    // the last ones announced
            Downloads                                            downloads;          // the last ones announced
            std::size_t downloads_done = 0; // finished since the queue was last empty
            std::chrono::steady_clock::time_point                next_resource_log;
            std::uint64_t                                        memory_max = 0; // of the player since the start
            std::string                                          memory_max_time;
            int                                                  threads_max = 0;

            /// CPU time of the player at a moment, for the load until the next one
            struct CpuSample
            {
                std::chrono::nanoseconds              used{};
                std::chrono::steady_clock::time_point at;
                std::uint64_t                         system_busy  = 0; // of all cores, total 0: unknown
                std::uint64_t                         system_total = 0;
                std::optional<std::uint64_t>          throttle_events{}; // Intel
                std::chrono::steady_clock::time_point thermal_at{};     // the sensors read; epoch: never
                std::optional<int>                    temperature_cpu{}; // what they told then
                std::optional<int>                    temperature_ram{};
                std::optional<int>                    temperature_ssd{};
                std::optional<bool>                   throttling{};
            };
            std::optional<CpuSample> cpu_of_debug_info; // the last debugInfo()
            std::optional<CpuSample> cpu_of_log;        // the last resource log
            std::optional<int>       cpu_system_of_log; // the load of the system over the 5 minutes of the last log
            std::optional<int>       cpu_max;
            std::string              cpu_max_time;
            std::chrono::steady_clock::time_point started_at;      // for the uptime of the player
            std::optional<std::chrono::seconds>   started_at_boot; // uptime of the device at the start

            void dispatch(const downloader::DownloadResult &result);
            void newIndex(smil::Document document, std::string base);
            void onTask(tasks::TaskAction action);
            void collectEngine();
            void findBackgrounds(std::map<std::string, std::string, std::less<>> *found);
            void planReboot(std::chrono::seconds not_before = {});
            void announceDeviceSettings();
            void announceDownloads();
            void reboot(const std::string &task_id);
            void logResources();
            void trackProcess(DebugInfo &info, std::optional<CpuSample> &since);
            std::expected<void, std::string> restartServer(bool enabled, std::uint16_t port);
    };
}

#endif // GARLIC_CORE_PLAYER_RUNTIME_PLAYER_RUNTIME_API_HPP
