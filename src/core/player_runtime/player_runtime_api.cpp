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
#include "player_runtime/player_runtime_api.hpp"

#include "common/time_format.hpp"
#include "content/index_manager.hpp"
#include "database/inventory_table.hpp"
#include "downloader/download_queue.hpp"
#include "httpd/router.hpp"
#include "logger/logger.hpp"
#include "player_runtime/reboot_schedule.hpp"
#include "rest_api/rest_api.hpp"

#if GARLIC_CORE_WITH_HTTPD
    #include "httpd/mhd_server.hpp"
#endif
#if GARLIC_CORE_WITH_SYSTEM
    #include "system/system_info.hpp"
#endif

#include <format>
#include <limits>

namespace garlic::core
{
    namespace
    {
        logger::LoggerConfig loggerConfig(const std::string &log_dir)
        {
            logger::LoggerConfig c;
            c.log_dir = log_dir;
            return c;
        }
    }

    PlayerRuntimeApi::PlayerRuntimeApi(common::FileSystem &f, const common::Clock &c, downloader::HttpTransport &t,
                                       PlayerRuntimeConfig cfg)
        : fs(f), clock(c), transport(t), settings(std::move(cfg)), config(f, c, settings.ini_path),
          logger(std::make_unique<logger::Logger>(f, c, loggerConfig(settings.log_dir)))
    {
    }

    PlayerRuntimeApi::~PlayerRuntimeApi()
    {
        if (server)
            server->stop();
        if (engine)
            engine->stop();
    }

    // ------------------------------------------------------------------ start

    /**
     * garlic-lib: LibFacade::init and initParser. The modules that need the loaded configuration (user agent, cache
     * directory, httpd settings) are created here.
     */
    std::expected<void, std::string> PlayerRuntimeApi::start()
    {
        if (auto loaded = config.load(); !loaded)
            return std::unexpected("configuration: " + loaded.error());
        config.setVersion(settings.version);
        config.setCacheDir(settings.cache_dir);
        config.setLogDir(settings.log_dir);
        config.determineIndexUri(settings.index_uri);
        (void)fs.createDirectories(settings.cache_dir);

        inventory = std::make_unique<database::InventoryTable>(clock);
        std::string database = settings.database_path.empty() ? settings.log_dir + "/garlic.db" : settings.database_path;
        if (auto opened = inventory->open(database); !opened)
            return std::unexpected("database: " + opened.error());

        auto free_space = settings.free_space;
#if GARLIC_CORE_WITH_SYSTEM
        if (!free_space)
            free_space = [cache = settings.cache_dir]
            {
                system::DiskSpace disk = system::diskSpace(cache);
                return disk.total > 0 ? disk.free : std::numeric_limits<std::uint64_t>::max(); // unknown: no cleanup
            };
#endif
        content::MediaManager::MakeSpace make_space;
        if (free_space)
        {
            cache_space = std::make_unique<content::CacheSpace>(fs, *inventory, *logger, settings.cache_dir, free_space);
            make_space  = [this](std::uint64_t bytes) { return cache_space->makeSpace(bytes); };
        }

        downloader::DownloaderConfig download;
        download.cache_dir  = settings.cache_dir;
        download.user_agent = config.userAgent();
        download.make_space = make_space;
        queue = std::make_unique<downloader::DownloadQueue>(transport, fs, *inventory, clock, download,
                                                            settings.parallel_downloads,
                                                            [this](const downloader::DownloadResult &r) { dispatch(r); });

        media = std::make_unique<content::MediaManager>(*queue, *inventory, fs, *logger, settings.cache_dir,
                                                        settings.supports, make_space);
        index = std::make_unique<content::IndexManager>(*queue, fs, clock, *logger, config, settings.cache_dir,
                                                        [this](smil::Document d, std::string base)
                                                        { newIndex(std::move(d), std::move(base)); });

        auto system_info = settings.system_info;
#if GARLIC_CORE_WITH_SYSTEM
        if (!system_info)
            system_info = [cache = settings.cache_dir, started = common::isoLocalTime(clock.now())]
            { return system::collectSystemInfo(cache, started); };
#endif
        settings.system_info = system_info;
        reporter = std::make_unique<reports::Reporter>(transport, fs, clock, *logger, config, *inventory,
                                                       settings.cache_dir, system_info);
        tasks = std::make_unique<tasks::TaskScheduler>(*queue, fs, *logger, config, settings.cache_dir,
                                                       [this](tasks::TaskAction a) { onTask(std::move(a)); });
        tasks->checkPending();

        router   = std::make_unique<httpd::Router>();
        rest_api::RestActions actions;
        actions.notify        = [this](std::string_view name) { notify(name); };
        actions.reboot        = [this](std::string task_id) { events.emplace_back(tasks::Reboot{std::move(task_id)}); };
        actions.reloadIndex   = [this] { index->start(); };
        actions.playTemporary = [this](std::string uri) { index->startTemporary(std::move(uri)); };
        actions.screenshot    = [this](std::string path) { events.emplace_back(Screenshot{std::move(path)}); };
        if (cache_space)
            actions.makeSpace = [this](std::uint64_t bytes) -> std::uint64_t
            {
                std::uint64_t needed = cache_space->needed(bytes); // garlic-lib named the bytes to free
                return needed == 0 || cache_space->makeSpace(bytes) ? 0 : needed;
            };
        rest_api = std::make_unique<rest_api::RestApi>(config, *inventory, fs, clock, std::move(actions));
        rest_api->registerRoutes(*router);
        auto httpd = config.httpd();
        if (auto started = restartServer(httpd.enabled, httpd.port); !started)
            logger->error("Httpd", "not started: " + started.error());

        planReboot();
        next_resource_log = clock.monotonic() + resource_log;
        index->start();
        return {};
    }

    // ------------------------------------------------------------------ poll

    std::vector<CoreEvent> PlayerRuntimeApi::poll()
    {
        transport.poll();
        if (queue)
            queue->poll();
        if (index)
            index->poll();
        if (reporter)
            reporter->poll();
        if (rest_api)
            rest_api->poll(); // screenshots that did not come in time
        if (server)
            server->poll();

        auto now = clock.monotonic();
        if (next_reboot && now >= *next_reboot)
        {
            events.emplace_back(tasks::Reboot{"DailyReboot"}); // garlic-lib: reboot::Timer
            planReboot(std::chrono::minutes(1));
        }
        if (now >= next_resource_log)
        {
            logResources();
            next_resource_log = now + resource_log;
        }

        collectEngine();
        return std::exchange(events, {});
    }

    std::optional<std::chrono::steady_clock::time_point> PlayerRuntimeApi::nextDeadline() const
    {
        auto now = clock.monotonic();
        if (!events.empty())
            return now;

        std::optional<std::chrono::steady_clock::time_point> next = next_resource_log;
        auto earlier = [&](std::optional<std::chrono::steady_clock::time_point> t)
        {
            if (t && (!next || *t < *next))
                next = t;
        };
        if (engine)
            earlier(engine->nextDeadline());
        if (index)
            earlier(index->nextDeadline());
        if (reporter)
            earlier(reporter->nextDeadline());
        if (rest_api)
            earlier(rest_api->nextDeadline());
        earlier(next_reboot);
        // curl and libmicrohttpd run without threads of their own: they need to be polled while they work
        if ((queue && !queue->isIdle()) || server)
            earlier(now + busy_poll);
        return next;
    }

    // ------------------------------------------------------------------ input from the frontend

    void PlayerRuntimeApi::mediaFinished(smil::InstanceId id)
    {
        if (engine)
            engine->mediaFinished(id);
    }

    void PlayerRuntimeApi::mediaFailed(smil::InstanceId id)
    {
        if (engine)
            engine->mediaFailed(id);
    }

    void PlayerRuntimeApi::elementActivated(smil::InstanceId id)
    {
        if (engine)
            engine->elementActivated(id);
    }

    void PlayerRuntimeApi::accessKey(char key)
    {
        if (engine)
            engine->accessKey(key);
    }

    void PlayerRuntimeApi::notify(std::string_view name)
    {
        if (engine)
            engine->notify(name);
    }

    void PlayerRuntimeApi::screenshotTaken(std::string_view path)
    {
        if (rest_api)
            rest_api->screenshotTaken(path);
    }

    void PlayerRuntimeApi::stop()
    {
        if (!engine)
            return;
        engine->stop();
        collectEngine();
    }

    // ------------------------------------------------------------------ settings

    PlayerSettings PlayerRuntimeApi::playerSettings() const
    {
        httpd::HttpdSettings httpd = config.httpd();
        return {config.playerName(), config.indexUri(), httpd.enabled, httpd.port};
    }

    std::expected<void, std::string> PlayerRuntimeApi::changePlayerSettings(const PlayerSettings &next)
    {
        auto uri = configuration::Configuration::validateIndexUri(next.index_uri);
        if (!uri)
            return std::unexpected(uri.error());
        if (next.httpd_enabled && next.httpd_port == 0)
            return std::unexpected("invalid port");

        PlayerSettings current = playerSettings();
        if (next.httpd_enabled != current.httpd_enabled || (next.httpd_enabled && next.httpd_port != current.httpd_port))
            if (auto restarted = restartServer(next.httpd_enabled, next.httpd_port); !restarted)
                return restarted; // nothing stored, the old server runs on

        config.setPlayerName(next.player_name);
        config.setIndexUri(*uri);
        config.setValue("httpd/enabled", next.httpd_enabled ? "true" : "false");
        config.setValue("httpd/port", std::to_string(next.httpd_port));
        if (queue)
            queue->setUserAgent(config.userAgent()); // the reports read it at every upload
        if (index)
            index->start();
        return {};
    }

    /// Starts the new server before the old one stops, so a port in use keeps the old one (and its port) running.
    std::expected<void, std::string> PlayerRuntimeApi::restartServer(bool enabled, std::uint16_t port)
    {
#if GARLIC_CORE_WITH_HTTPD
        std::unique_ptr<httpd::Server> next;
        if (enabled)
        {
            httpd::HttpdSettings httpd = config.httpd(); // the other keys of [httpd]; enabled and port are new
            httpd.enabled              = true;
            httpd.port                 = port;
            next                       = std::make_unique<httpd::MhdServer>(*router, fs);
            if (auto started = next->start(httpd); !started)
                return started;
        }
        if (server)
            server->stop();
        server = std::move(next);
#else
        (void)enabled;
        (void)port;
#endif
        return {};
    }

    // ------------------------------------------------------------------ inside

    /// The DownloadQueue has one callback: the index, the files of the task scheduler and the media.
    void PlayerRuntimeApi::dispatch(const downloader::DownloadResult &result)
    {
        if (index && !index->url().empty() && result.url == index->url())
            index->downloaded(result);
        else if (tasks && tasks->downloaded(result))
            return;
        else if (media)
        {
            media->downloaded(result);
            findBackgrounds(nullptr);
        }
    }

    /**
     * A new valid index (garlic-lib: loadIndex, without stopping before the new one is known to be valid): the old
     * engine stops (StopMedia), the new document is loaded by the media manager, the reports and tasks follow its
     * subscriptions, then NewLayout and the new engine. The old document lives as long as the frontend keeps the
     * previous NewLayout.
     */
    void PlayerRuntimeApi::newIndex(smil::Document next, std::string base)
    {
        if (engine)
        {
            engine->stop();
            collectEngine();
            engine.reset();
        }
        document = std::make_shared<smil::Document>(std::move(next));

        media->load(*document, base);
        reporter->subscribe(document->head.subscriptions);
        tasks->subscribe(document->head.subscriptions);

        missing_backgrounds.clear();
        for (const smil::Region &region : document->head.regions)
            missing_backgrounds.insert(region.background_image);
        missing_backgrounds.insert(document->head.root_layout.background_image);
        for (const char *nothing : {"", "none", "inherited"})
            missing_backgrounds.erase(nothing);
        NewLayout layout{document, {}};
        findBackgrounds(&layout.backgrounds);
        events.emplace_back(std::move(layout));
        smil::EngineConfig engine_config;
        engine_config.player_id   = config.uuid();
        engine_config.player_name = config.playerName();
        engine_config.random_seed = settings.random_seed;
        engine = std::make_unique<smil::Engine>(*document, clock, *media, *logger, engine_config);
        engine->start();
    }

    /// garlic-lib: changeConfig, reboot, applyCommand
    void PlayerRuntimeApi::onTask(tasks::TaskAction action)
    {
        if (std::holds_alternative<tasks::ApplyConfiguration>(action))
        {
            planReboot(); // the restart times may have changed
            index->start(); // the index URI may have changed
            return;
        }
        if (const auto *clear = std::get_if<tasks::ClearCaches>(&action); clear && clear->player)
        {
            // garlic-lib: FreeDiscSpace::clearPlayerCache; the host restarts afterwards
            inventory->clear();
            (void)fs.removeAll(settings.cache_dir);
            (void)fs.createDirectories(settings.cache_dir);
        }
        std::visit([this](auto &a)
                   {
                       if constexpr (!std::is_same_v<std::decay_t<decltype(a)>, tasks::ApplyConfiguration>)
                           events.emplace_back(std::move(a));
                   },
                   action);
    }

    /// The background images of the layout that are there now: into found (NewLayout) or as BackgroundImage events.
    void PlayerRuntimeApi::findBackgrounds(std::map<std::string, std::string, std::less<>> *found)
    {
        for (auto it = missing_backgrounds.begin(); it != missing_backgrounds.end();)
        {
            auto path = media->locate(*it);
            if (!path)
            {
                ++it;
                continue;
            }
            if (found)
                found->emplace(*it, std::move(*path));
            else
                events.emplace_back(BackgroundImage{*it, std::move(*path)});
            it = missing_backgrounds.erase(it);
        }
    }

    void PlayerRuntimeApi::collectEngine()
    {
        if (!engine)
            return;
        for (smil::Command &command : engine->poll())
            std::visit([this](auto &c) { events.emplace_back(std::move(c)); }, command);
    }

    /// The next scheduled restart from schedule/reboot_days and schedule/reboot_time, in local time.
    void PlayerRuntimeApi::planReboot(std::chrono::seconds not_before)
    {
        auto local_now = common::toLocalTime(clock.now()) + not_before;
        auto next      = player_runtime::nextReboot(config.rebootDays(), config.rebootTime(), local_now);
        if (!next)
        {
            next_reboot.reset();
            return;
        }
        next_reboot = clock.monotonic() + not_before + (*next - local_now);
    }

    /// garlic-lib: LibFacade::timerEvent with the ResourceMonitor
    void PlayerRuntimeApi::logResources()
    {
        if (!settings.system_info)
            return;
        reports::SystemInfo info = settings.system_info();
        logger->debug("Develop", std::format("disc {}/{} memory {}/{}", info.free_space.value_or(0),
                                             info.total_capacity.value_or(0), info.memory_used.value_or(0),
                                             info.memory_total.value_or(0)));
    }
}
