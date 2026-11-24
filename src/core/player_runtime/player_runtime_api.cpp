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
#include "common/version.hpp"
#include "content/index_manager.hpp"
#include "database/inventory_table.hpp"
#include "downloader/download_queue.hpp"
#include "httpd/router.hpp"
#include "httpd/server.hpp"
#include "logger/logger.hpp"
#include "player_runtime/reboot_schedule.hpp"
#include "rest_api/rest_api.hpp"

#if GARLIC_CORE_WITH_HTTPD
    #include "httpd/mhd_server.hpp"
#endif
#if GARLIC_CORE_WITH_SYSTEM
    #include "system/system_info.hpp"
#endif

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <limits>
#include <thread>

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

#if GARLIC_CORE_WITH_SYSTEM
        /// The temperatures of the device; CPU temperature and throttling of the supervisor only where the core reads
        /// none itself
        system::Thermal thermalWithFallback(std::optional<int> cpu_fallback, std::optional<bool> throttling_fallback)
        {
            system::Thermal thermal = system::thermal();
            if (!thermal.cpu)
                thermal.cpu = cpu_fallback;
            if (!thermal.throttling && !thermal.throttle_events) // Intel compares its counters itself
                thermal.throttling = throttling_fallback;
            return thermal;
        }
#endif
    }

    PlayerRuntimeApi::PlayerRuntimeApi(common::FileSystem &f, const common::Clock &c, downloader::HttpTransport &t,
                                       PlayerRuntimeConfig cfg)
        : fs(f), clock(c), transport(t), settings(std::move(cfg)), config(f, c, settings.ini_path),
          logger(std::make_unique<logger::Logger>(f, c, loggerConfig(settings.log_dir))), started_at(c.monotonic())
    {
#if GARLIC_CORE_WITH_SYSTEM
        started_at_boot = system::uptime();
#endif
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
        if (auto loaded = config.load(settings.uuid); !loaded)
            return std::unexpected("configuration: " + loaded.error());
        config.setVersion(settings.version);
        config.setLauncher(settings.launcher);
        config.setCacheDir(settings.cache_dir);
        config.setLogDir(settings.log_dir);
        config.determineIndexUri(settings.index_uri);
        (void)fs.createDirectories(settings.cache_dir);

        inventory = std::make_unique<database::InventoryTable>(clock);
        std::string database = settings.database_path.empty() ? settings.log_dir + "/garlic.db" : settings.database_path;
        if (auto opened = inventory->open(database); !opened)
            return std::unexpected("database: " + opened.error());
        removeBrokenObjects();

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
            auto current = [this] { return media ? media->currentFiles() : std::set<std::string>{}; };
            cache_space  = std::make_unique<content::CacheSpace>(fs, clock, *inventory, *logger, settings.cache_dir, free_space,
                                                                 current);
            make_space   = [this](std::uint64_t bytes) { return cache_space->makeSpace(bytes); };
        }

        downloader::DownloaderConfig download;
        download.cache_dir  = settings.cache_dir;
        download.user_agent = config.userAgent();
        download.make_space = make_space;
        queue = std::make_unique<downloader::DownloadQueue>(transport, fs, *inventory, clock, download,
                                                            settings.parallel_downloads,
                                                            [this](const downloader::DownloadResult &r) { dispatch(r); });

        media = std::make_unique<content::MediaManager>(*queue, *inventory, fs, clock, *logger, settings.cache_dir,
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
        // the reports with the load of the system from the resource log, a 5-minute average (empty before the first)
        auto report_info = [this, system_info]
        {
            reports::SystemInfo info = system_info ? system_info() : reports::SystemInfo{};
                    if (!info.cpu_usage) // the system, where it cannot be read (Android 8+) the player
                info.cpu_usage = cpu_system_of_log ? cpu_system_of_log : cpu_player_of_log;
            if (info.hdmi_output.empty())
                info.hdmi_output = display_mode;
#if GARLIC_CORE_WITH_SYSTEM
            if (!info.cpu_temperature)
                info.cpu_temperature = thermalWithFallback(temperature_cpu_fallback, throttling_fallback).cpu; // as the information dialog
#endif
            info.display_model      = display_hardware[0];
            info.display_product_id = display_hardware[1];
            info.display_serial     = display_hardware[2];
            info.display_vendor     = display_hardware[3];
            info.own = ownReportValues();
            return info;
        };
        reporter = std::make_unique<reports::Reporter>(transport, fs, clock, *logger, config, *inventory,
                                                       settings.cache_dir, report_info);
        tasks = std::make_unique<tasks::TaskScheduler>(*queue, fs, *logger, config, settings.cache_dir,
                                                       [this](tasks::TaskAction a) { onTask(std::move(a)); });
        tasks->checkPending();

        // -c config.xml (garlic-lib: setConfigFromExternal before the first index). An index URI of the command line
        // still wins, a new player name changes the user agent. Missing or broken: an event, the player goes on.
        if (!settings.configuration_file.empty())
        {
            tasks->applyConfigurationFile(settings.configuration_file);
            config.determineIndexUri(settings.index_uri);
            queue->setUserAgent(config.userAgent());
        }

        router   = std::make_unique<httpd::Router>();
        rest_api::RestActions actions;
        actions.notify        = [this](std::string_view name) { notify(name); };
        actions.reboot        = [this](std::string task_id) { reboot(task_id); };
        actions.reloadIndex   = [this] { index->start(); };
        actions.playTemporary = [this](std::string uri) { index->startTemporary(std::move(uri)); };
        actions.screenshot    = [this](std::string path) { events.emplace_back(Screenshot{std::move(path)}); };
        // IAdea: OBJECT_REMOVED for a file removed on purpose, LRU_OBJECT_REMOVED is the cleanup of CacheSpace
        actions.removed = [this](std::string uri)
        { logger->event(logger::Severity::Informational, "ContentManager", "OBJECT_REMOVED", {{"resourceURI", uri}}); };
        actions.configurationChanged = [this]
        {
            queue->setUserAgent(config.userAgent()); // a new player name
            onTask(tasks::ApplyConfiguration{});
        };
        actions.deviceSettingsChanged = [this] { announceDeviceSettings(); };
        actions.screenPower           = [this](bool on) { events.emplace_back(ScreenPower{on}); };
        actions.serialNumber          = [this]
        {
            // the MAC of the first ethernet interface, else of the first one with a MAC
            if (!settings.system_info)
                return std::string{};
            reports::SystemInfo info = settings.system_info();
            std::string         first;
            for (const auto &interface : info.network)
            {
                if (interface.mac.empty())
                    continue;
                if (interface.type == "ethernet")
                    return interface.mac;
                if (first.empty())
                    first = interface.mac;
            }
            return first;
        };
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
        announceDeviceSettings();
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
            reboot("DailyReboot"); // garlic-lib: reboot::Timer
            planReboot(std::chrono::minutes(1));
        }
        if (now >= next_resource_log)
        {
            logResources();
            next_resource_log = now + resource_log;
        }

        announceDownloads();
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
            if (t && *t < *next) // next starts with the resource log
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
        if ((queue && !queue->isIdle()) || (reporter && reporter->isBusy()) || server)
            earlier(now + busy_poll);
        return next;
    }

    // ------------------------------------------------------------------ input from the frontend

    void PlayerRuntimeApi::mediaFinished(smil::InstanceId id)
    {
        if (engine)
            engine->mediaFinished(id);
    }

    void PlayerRuntimeApi::mediaFailed(smil::InstanceId id, smil::MediaError error, std::string_view reason)
    {
        if (engine)
            engine->mediaFailed(id, error, reason);
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

    void PlayerRuntimeApi::cachesCleared()
    {
        if (auto task_id = std::exchange(reboot_after_clear, std::nullopt))
            reboot(*task_id);
    }

    void PlayerRuntimeApi::logWarning(std::string_view category, std::string_view message)
    {
        logger->warning(category, message);
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
        server = std::move(next); // the old one stops in its destructor
#else
        (void)enabled;
        (void)port;
#endif
        return {};
    }

    // ------------------------------------------------------------------ inside

    /// The DownloadQueue has one callback: the index, the files of the task scheduler and the media. It comes from
    /// poll() after start(), so all of them are there.
    void PlayerRuntimeApi::dispatch(const downloader::DownloadResult &result)
    {
        logDownload(result);
        if (media->isMedia(result.url))
            ++downloads_done;
        if (result.url == index->url())
            index->downloaded(result);
        else if (tasks->downloaded(result))
            return;
        else
        {
            media->downloaded(result);
            findBackgrounds(nullptr);
        }
    }

    /**
     * At the start (IAdea: BROKEN_OBJECT_REMOVED): a complete file of the inventory whose size is not the one the
     * server told, or that is gone (a crash while writing, a cleaned card), is removed with its inventory row, so it
     * is downloaded again instead of played broken or asked for as not modified. An update waiting as ".ready" is the
     * one checked. Without a known size (content_length 0) nothing can be checked.
     */
    void PlayerRuntimeApi::removeBrokenObjects()
    {
        for (const database::InventoryRow &row : inventory->findAll())
        {
            if (row.state != downloader::InventoryState::Complete || row.content_length == 0 || row.file_name.empty())
                continue;
            std::string file  = settings.cache_dir + "/" + row.file_name;
            std::string ready = file + ".ready";
            std::string path  = fs.exists(ready) ? ready : file;
            auto        size  = fs.fileSize(path);
            if (size && *size == row.content_length)
                continue;
            logger::Metadata data{{"resourceURI", row.url}};
            if (auto modified = fs.lastModified(path))
                data.emplace_back("lastModifiedTime", common::isoLocalTime(*modified));
            data.emplace_back("contentLength", std::to_string(row.content_length));
            data.emplace_back("transferLength", size ? std::to_string(*size) : "-1");
            (void)fs.remove(path);
            inventory->deleteByCacheName(row.file_name);
            logger->event(logger::Severity::Warning, "ContentManager", "BROKEN_OBJECT_REMOVED", data);
        }
    }

    /**
     * Events of every download, index, media, task files and firmware alike, as garlic-lib (Downloader) and IAdea:
     * OBJECT_UPDATED for a new file, FETCH_FAILED for a failure, a warning if a cached copy plays on, an error if
     * there is none. A digest of the server that does not match: FETCH_FAILED with E_CHECKSUM_NOT_MATCHED and the
     * calculated digest (meta md5 as IAdea, or sha256). Without space it is DISK_FULL of the MediaManager instead
     * (garlic-lib: FETCH_ERROR). Not modified and not cacheable: nothing. transferLength of garlic-lib is missing: the
     * downloader does not tell it.
     */
    void PlayerRuntimeApi::logDownload(const downloader::DownloadResult &result)
    {
        using downloader::DownloadStatus;
        auto file = [this](const std::string &path, std::string_view size_key, std::string_view time_key)
        {
            logger::Metadata data;
            if (auto size = fs.fileSize(path))
                data.emplace_back(size_key, std::to_string(*size));
            if (auto modified = fs.lastModified(path))
                data.emplace_back(time_key, common::isoLocalTime(*modified));
            return data;
        };
        if (result.status == DownloadStatus::Downloaded && !result.local_path.empty())
        {
            // an update of a cached file waits next to it until it is taken
            std::string ready = result.local_path + ".ready";
            std::string path  = fs.exists(ready) ? ready : result.local_path;
            logger::Metadata data{{"resourceURI", result.url}};
            for (auto &entry : file(path, "contentLength", "lastModifiedTime"))
                data.push_back(std::move(entry));
            logger->event(logger::Severity::Informational, "ContentManager", "OBJECT_UPDATED", data);
        }
        else if (result.status == DownloadStatus::Failed && !result.checksum.empty())
        {
            // as IAdea: FETCH_FAILED : E_CHECKSUM_NOT_MATCHED with the calculated digest under its algorithm
            logger->event(logger::Severity::Error, "ContentManager", "FETCH_FAILED",
                          {{"resourceURI", result.url},
                           {"xmp:errorMessage", "E_CHECKSUM_NOT_MATCHED"},
                           {result.checksum_algorithm, result.checksum}});
        }
        else if (result.status == DownloadStatus::Failed && !result.no_space)
        {
            logger::Metadata data{{"resourceURI", result.url}, {"errorMessage", result.error}};
            bool cached = !result.local_path.empty();
            if (cached)
                for (auto &entry : file(result.local_path, "lastCachedLength", "lastCachedModifiedTime"))
                    data.push_back(std::move(entry));
            logger->event(cached ? logger::Severity::Warning : logger::Severity::Error, "ContentManager",
                          "FETCH_FAILED", data);
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
        // the preview in garlic-hub only plays: no reports to the hub, no tasks from it
        if (!settings.preview)
        {
            reporter->subscribe(document->head.subscriptions);
            tasks->subscribe(document->head.subscriptions);
        }

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

    bool PlayerRuntimeApi::applyConfigurationFile(std::string_view path)
    {
        if (!tasks || !tasks->applyConfigurationFile(path))
            return false;
        queue->setUserAgent(config.userAgent()); // a new player name
        onTask(tasks::ApplyConfiguration{});
        return true;
    }

    std::optional<std::chrono::seconds> PlayerRuntimeApi::remainingTime(smil::InstanceId id) const
    {
        if (!engine)
            return std::nullopt;
        auto end = engine->plannedEnd(id);
        if (!end)
            return std::nullopt;
        auto rest = std::max(*end - clock.monotonic(), std::chrono::steady_clock::duration::zero());
        return std::chrono::ceil<std::chrono::seconds>(rest);
    }

    /// garlic-lib: changeConfig, reboot, applyCommand
    void PlayerRuntimeApi::onTask(tasks::TaskAction action)
    {
        if (std::holds_alternative<tasks::ApplyConfiguration>(action))
        {
            planReboot(); // the restart times may have changed
            announceDeviceSettings();
            index->start(); // the index URI may have changed
            return;
        }
        if (const auto *task = std::get_if<tasks::Reboot>(&action))
        {
            reboot(task->task_id);
            return;
        }
        if (const auto *install = std::get_if<tasks::InstallSoftware>(&action))
        {
            if (settings.supervisor)
                settings.supervisor->installSoftware(install->path);
            else
                logger->warning("Supervisor", "none, software not installed: " + install->path);
            return;
        }
        auto &clear = std::get<tasks::ClearCaches>(action);
        if (clear.player)
        {
            // garlic-lib: FreeDiscSpace::clearPlayerCache. One event for all instead of OBJECT_REMOVED per file: the
            // cache may hold hundreds
            media->holdDownloads(); // missing files are not downloaded again until the restart
            std::uint64_t bytes = 0;
            auto          rows  = inventory->findAll();
            for (const database::InventoryRow &row : rows)
                bytes += row.content_length;
            logger->event(logger::Severity::Informational, "ContentManager", "OBJECTS_REMOVED",
                          {{"information", "player cache cleared: " + std::to_string(rows.size()) + " files, " +
                                               std::to_string(bytes) + " Bytes deleted"}});
            inventory->clear();
            (void)fs.removeAll(settings.cache_dir);
            (void)fs.createDirectories(settings.cache_dir);
        }
        // garlic-lib: applyCommand restarts after clearing, "otherwise a black screen"; first the frontend clears the
        // web cache, cachesCleared() then restarts
        reboot_after_clear = clear.task_id;
        events.emplace_back(std::move(clear));
    }

    namespace
    {
        /// "80", "80%", " 80 % " → 80, clamped to 0…100; anything else (empty, "loud") → nullopt
        std::optional<int> percent(std::string_view text)
        {
            while (!text.empty() && text.front() == ' ')
                text.remove_prefix(1);
            int  value = 0;
            auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc() || end == text.data())
                return std::nullopt;
            std::string_view rest(end, static_cast<std::size_t>(text.data() + text.size() - end));
            while (!rest.empty() && (rest.front() == ' ' || rest.front() == '%'))
                rest.remove_prefix(1);
            if (!rest.empty())
                return std::nullopt;
            return std::clamp(value, 0, 100);
        }
    }

    /// A DeviceSettings event when master volume or brightness differ from the last ones announced.
    void PlayerRuntimeApi::announceDeviceSettings()
    {
        DeviceSettings now{percent(config.value("audio/volume")), percent(config.value("display/brightness"))};
        if (now == device_settings)
            return;
        device_settings = now;
        events.emplace_back(now);
    }

    /// A Downloads event when the media files changed or the bytes of the first by at least a percent; only media
    /// count (not the index, not task files), so the frontend shows "Loading 1 / X" before the first medium. The
    /// count starts again when no medium is downloading.
    void PlayerRuntimeApi::announceDownloads()
    {
        auto        is_media  = [this](std::string_view url) { return media && media->isMedia(url); };
        std::size_t remaining = queue ? queue->count(is_media) : 0;
        if (remaining == 0)
            downloads_done = 0;
        auto      progress = queue ? queue->firstProgress(is_media) : downloader::Downloader::Progress{};
        Downloads now{downloads_done, downloads_done + remaining, progress.received, progress.total};
        auto percent = [](const Downloads &d) { return d.bytes_total == 0 ? 0 : d.bytes_done * 100 / d.bytes_total; };
        if (now.done == downloads.done && now.total == downloads.total && now.bytes_total == downloads.bytes_total &&
            percent(now) == percent(downloads))
            return;
        downloads = now;
        events.emplace_back(now);
    }

    void PlayerRuntimeApi::reboot(const std::string &task_id)
    {
        if (settings.supervisor)
            settings.supervisor->reboot(task_id);
        else
            logger->warning("Supervisor", "none, no restart: " + task_id);
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
        DebugInfo process;
        trackProcess(process, cpu_of_log);
        cpu_system_of_log = process.cpu_system;
        cpu_player_of_log = process.cpu;
        if (process.cpu && (!cpu_max || *process.cpu > *cpu_max)) // the average of the 5 minutes since the last log
        {
            cpu_max      = process.cpu;
            cpu_max_time = common::isoLocalTime(clock.now());
        }
        if (!settings.system_info)
            return;
        reports::SystemInfo info = settings.system_info();
        auto percent = [](const std::optional<int> &value) { return value ? std::to_string(*value) + "%" : "-"; };
        auto celsius = [](const std::optional<int> &value) { return value ? std::to_string(*value) + "°C" : "-"; };
        logger->debug("Develop",
                      std::format("disc {}/{} memory {}/{} player {} (max {}) threads {} (max {}) cpu {} (max {}) system {}"
                                  " temperature cpu {} ram {} ssd {} throttling {}",
                                  info.free_space.value_or(0), info.total_capacity.value_or(0),
                                  info.memory_used.value_or(0), info.memory_total.value_or(0), process.memory_used,
                                  memory_max, process.threads, threads_max, percent(process.cpu), percent(cpu_max),
                                  percent(process.cpu_system), celsius(process.temperature_cpu),
                                  celsius(process.temperature_ram), celsius(process.temperature_ssd),
                                  !process.throttling ? "-" : *process.throttling ? "true" : "false"));
    }

    /**
     * Memory, threads and CPU load of the player and their maxima (garlic-lib: ResourceMonitor::refresh). The load is
     * that since the sample in since, of the whole CPU: the CPU time of the player and its processes through the time
     * that passed times the cores. That of the system: its busy through its total time of all cores. Temperatures and
     * throttling of the device as the platform tells them, read at most every 5 seconds (like the launchers in the
     * frontends); the throttle counters of Intel against the sample in since.
     */
    void PlayerRuntimeApi::trackProcess(DebugInfo &info, std::optional<CpuSample> &since)
    {
#if GARLIC_CORE_WITH_SYSTEM
        system::ProcessInfo process = system::processInfo();
        info.memory_used            = process.memory;
        info.threads                = process.threads;
        CpuSample now{process.cpu_time.value_or(std::chrono::nanoseconds{}), clock.monotonic()};
        if (process.system_cpu)
        {
            now.system_busy  = process.system_cpu->busy;
            now.system_total = process.system_cpu->total;
        }
        if (since && process.cpu_time && now.at > since->at && now.used >= since->used)
        {
            double cores   = std::max(1U, std::thread::hardware_concurrency());
            double used    = std::chrono::duration<double>(now.used - since->used).count();
            double elapsed = std::chrono::duration<double>(now.at - since->at).count();
            info.cpu       = static_cast<int>(std::lround(std::min(100.0, used * 100.0 / (elapsed * cores))));
        }
        if (since && now.system_total > since->system_total && now.system_busy >= since->system_busy)
        {
            double busy     = static_cast<double>(now.system_busy - since->system_busy);
            double total    = static_cast<double>(now.system_total - since->system_total);
            info.cpu_system = static_cast<int>(std::lround(std::min(100.0, busy * 100.0 / total)));
        }

        constexpr auto thermal_interval = std::chrono::seconds(5);
        if (since && since->thermal_at != std::chrono::steady_clock::time_point{} &&
            now.at - since->thermal_at < thermal_interval) // what the sensors told the last time
        {
            now.thermal_at      = since->thermal_at;
            now.throttle_events = since->throttle_events;
            now.temperature_cpu = since->temperature_cpu;
            now.temperature_ram = since->temperature_ram;
            now.temperature_ssd = since->temperature_ssd;
            now.throttling      = since->throttling;
        }
        else
        {
            system::Thermal thermal = thermalWithFallback(temperature_cpu_fallback, throttling_fallback);
            now.thermal_at          = now.at;
            now.throttle_events     = thermal.throttle_events;
            now.temperature_cpu     = thermal.cpu;
            now.temperature_ram     = thermal.ram;
            now.temperature_ssd     = thermal.ssd;
            now.throttling          = thermal.throttling;
            if (!now.throttling && thermal.throttle_events) // Intel: throttled since the previous read
                now.throttling = since && since->throttle_events && *thermal.throttle_events > *since->throttle_events;
        }
        info.temperature_cpu = now.temperature_cpu;
        info.temperature_ram = now.temperature_ram;
        info.temperature_ssd = now.temperature_ssd;
        info.throttling      = now.throttling;
        since                = now;
#else
        (void)since;
#endif
        if (info.memory_used > memory_max)
        {
            memory_max      = info.memory_used;
            memory_max_time = common::isoLocalTime(clock.now());
        }
        threads_max          = std::max(threads_max, info.threads);
        info.memory_max      = memory_max;
        info.memory_max_time = memory_max_time;
        info.threads_max     = threads_max;
        info.cpu_max         = cpu_max;
        info.cpu_max_time    = cpu_max_time;
    }

    std::chrono::seconds PlayerRuntimeApi::playerUptime(const std::optional<std::chrono::seconds> &system) const
    {
        // the clock of the device where known: the monotonic one stops in sleep, the player would look restarted
        if (system && started_at_boot && *system >= *started_at_boot)
            return *system - *started_at_boot;
        return std::chrono::duration_cast<std::chrono::seconds>(clock.monotonic() - started_at);
    }

    /**
     * The own values of the player in the system report, props "garlic.*" next to those of IAdea: uptime of player and
     * device (seconds), memory (bytes) and threads of the player with their maxima, its CPU load of the last resource
     * log, temperatures (°C) and throttling. Read now, without changing the samples of the debug dialog. Unknown: left
     * out.
     */
    std::vector<std::pair<std::string, std::string>> PlayerRuntimeApi::ownReportValues() const
    {
        std::vector<std::pair<std::string, std::string>> values;
        std::optional<std::chrono::seconds>              system_uptime;
#if GARLIC_CORE_WITH_SYSTEM
        system_uptime = system::uptime();
#endif
        values.emplace_back("garlic.uptime.player", std::to_string(playerUptime(system_uptime).count()));
        if (display_operating_hours)
            values.emplace_back("garlic.display.operating_hours", std::to_string(*display_operating_hours));
        if (system_uptime)
            values.emplace_back("garlic.uptime.system", std::to_string(system_uptime->count()));
#if GARLIC_CORE_WITH_SYSTEM
        system::ProcessInfo process = system::processInfo();
        if (process.memory > 0)
        {
            values.emplace_back("garlic.memory.player", std::to_string(process.memory));
            values.emplace_back("garlic.memory.player.max", std::to_string(std::max(memory_max, process.memory)));
        }
        if (process.threads > 0)
        {
            values.emplace_back("garlic.threads", std::to_string(process.threads));
            values.emplace_back("garlic.threads.max", std::to_string(std::max(threads_max, process.threads)));
        }
        if (cpu_player_of_log)
            values.emplace_back("garlic.cpu.player", std::to_string(*cpu_player_of_log) + "%");
        system::Thermal thermal = thermalWithFallback(temperature_cpu_fallback, throttling_fallback);
        auto celsius = [&values](const char *name, const std::optional<int> &value)
        {
            if (value)
                values.emplace_back(name, std::to_string(*value));
        };
        celsius("garlic.temperature.cpu", thermal.cpu);
        celsius("garlic.temperature.ram", thermal.ram);
        celsius("garlic.temperature.ssd", thermal.ssd);
        if (thermal.throttling)
            values.emplace_back("garlic.throttling", *thermal.throttling ? "true" : "false");
#endif
        return values;
    }

    void PlayerRuntimeApi::setDisplayMode(int width, int height, int refresh)
    {
        if (width <= 0 || height <= 0)
            display_mode.clear();
        else
            display_mode = std::to_string(width) + "x" + std::to_string(height) +
                           (refresh > 0 ? "p-" + std::to_string(refresh) : "");
    }

    void PlayerRuntimeApi::setDisplayHardware(std::string model, std::string product_id, std::string serial,
                                              std::string vendor)
    {
        display_hardware = {std::move(model), std::move(product_id), std::move(serial), std::move(vendor)};
    }

    DebugInfo PlayerRuntimeApi::debugInfo()
    {
        DebugInfo info;
        info.index_title = document ? document->head.title : std::string();
        info.user_agent  = config.userAgent();
        info.version     = common::version();
        if (settings.system_info)
        {
            reports::SystemInfo system = settings.system_info();
            info.disk_total            = system.total_capacity;
            info.disk_free             = system.free_space;
            info.memory_total          = system.memory_total;
            if (system.memory_total && system.memory_used)
                info.memory_free = *system.memory_total - std::min(*system.memory_used, *system.memory_total);
        }
        trackProcess(info, cpu_of_debug_info);
#if GARLIC_CORE_WITH_SYSTEM
        info.uptime_system = system::uptime();
#endif
        info.uptime_player = playerUptime(info.uptime_system);
        if (queue)
        {
            for (const auto &running : queue->running())
                info.downloads.push_back({media ? media->displayName(running.url) : running.url,
                                          running.progress.received, running.progress.total});
            info.downloads_waiting = queue->pendingCount();
        }
        return info;
    }
}
