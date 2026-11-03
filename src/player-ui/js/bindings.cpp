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
#include "common/clock.hpp"
#include "common/std_file_system.hpp"
#include "configuration/configuration.hpp"
#include "downloader/fetch_transport.hpp"
#include "player_runtime/player_runtime_api.hpp"
#include "smil/document.hpp"

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <climits>
#include <memory>

namespace player
{
    using namespace garlic::core;
    using emscripten::val;

    namespace
    {
        template <typename... Functions> struct Overloaded : Functions...
        {
            using Functions::operator()...;
        };

        std::string kindName(smil::MediaKind kind)
        {
            switch (kind)
            {
                case smil::MediaKind::Image: return "image";
                case smil::MediaKind::Video: return "video";
                case smil::MediaKind::Audio: return "audio";
                case smil::MediaKind::Web:
                case smil::MediaKind::Widget: return "web";
                case smil::MediaKind::Brush: return "brush";
                case smil::MediaKind::Command: return "command";
                case smil::MediaKind::Prefetch: return {};
            }
            return {};
        }

        std::string lower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        /// soundLevel "50%" → 0.5, else 1 (like the qwidget player)
        double volumeOf(const std::string &sound_level)
        {
            if (sound_level.size() < 2 || sound_level.back() != '%')
                return 1;
            double percent = 0;
            auto   parsed  = std::from_chars(sound_level.data(), sound_level.data() + sound_level.size() - 1, percent);
            if (parsed.ec != std::errc{})
                return 1;
            return std::clamp(percent / 100, 0.0, 1.0);
        }

        val optionalNumber(const std::optional<std::uint64_t> &value)
        {
            return value ? val(static_cast<double>(*value)) : val::null();
        }
    }

    /**
     * The bridge between the core and the JS player (player.js). It owns the core (PlayerRuntimeApi) with its file
     * system, clock and transport and turns the events into plain JS objects: no pointer into the core reaches JS.
     * No threads: JS calls poll() and plans the next call with msUntilNext().
     */
    class Player
    {
        public:
            /// options: {root, index, seed}; root is the directory for configuration, cache and logs
            explicit Player(val options)
            {
                root                 = options["root"].as<std::string>();
                config.ini_path      = root + "/config/garlic-player.ini";
                config.cache_dir     = root + "/cache";
                config.log_dir       = root + "/logs";
                config.version       = PLAYER_VERSION;
                config.index_uri     = options["index"].as<std::string>();
                config.random_seed   = options["seed"].as<std::uint32_t>();
                // widgets are directories in the cache, their relative links cannot be loaded from it yet
                config.supports = [](const smil::Media &media) { return media.kind != smil::MediaKind::Widget; };
            }

            /// Empty, or the error text.
            std::string start()
            {
                if (!config.index_uri.empty())
                {
                    auto valid = configuration::Configuration::validateIndexUri(config.index_uri);
                    if (!valid)
                        return valid.error();
                    config.index_uri = *valid;
                }
                (void)fs.createDirectories(root + "/config");
                (void)fs.createDirectories(config.log_dir);

                runtime = std::make_unique<PlayerRuntimeApi>(fs, clock, http, config);
                auto started = runtime->start();
                return started ? std::string() : started.error();
            }

            val poll()
            {
                val events = val::array();
                for (const CoreEvent &event : runtime->poll())
                {
                    val converted = toJs(event);
                    if (!converted.isNull())
                        events.call<void>("push", converted);
                }
                return events;
            }

            /// Milliseconds until poll() is needed again, -1: only after input.
            int msUntilNext() const
            {
                auto next = runtime->nextDeadline();
                if (!next)
                    return -1;
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(*next - clock.monotonic()).count();
                return static_cast<int>(std::clamp<long long>(ms, 0, INT_MAX));
            }

            std::string indexUri() const { return runtime->configuration().indexUri(); }

            void mediaFinished(int instance) { runtime->mediaFinished(static_cast<smil::InstanceId>(instance)); }
            void mediaFailed(int instance) { runtime->mediaFailed(static_cast<smil::InstanceId>(instance)); }
            void activated(int instance) { runtime->elementActivated(static_cast<smil::InstanceId>(instance)); }

            void accessKey(const std::string &text)
            {
                if (!text.empty())
                    runtime->accessKey(static_cast<char>(std::tolower(static_cast<unsigned char>(text.front()))));
            }

            void screenshotTaken(const std::string &path) { runtime->screenshotTaken(path); }
            void cachesCleared() { runtime->cachesCleared(); }
            void stop() { runtime->stop(); }

            val settings() const
            {
                PlayerSettings current = runtime->playerSettings();
                val            result  = val::object();
                result.set("playerName", current.player_name);
                result.set("indexUri", current.index_uri);
                result.set("httpdEnabled", current.httpd_enabled);
                result.set("httpdPort", current.httpd_port);
                return result;
            }

            /// Empty, or the error text of the core.
            std::string changeSettings(val values)
            {
                PlayerSettings next{values["playerName"].as<std::string>(), values["indexUri"].as<std::string>(),
                                    values["httpdEnabled"].as<bool>(),
                                    static_cast<std::uint16_t>(values["httpdPort"].as<int>())};
                auto changed = runtime->changePlayerSettings(next);
                return changed ? std::string() : changed.error();
            }

            val debugInfo()
            {
                DebugInfo info   = runtime->debugInfo();
                val       result = val::object();
                result.set("title", info.index_title);
                result.set("userAgent", info.user_agent);
                result.set("diskFree", optionalNumber(info.disk_free));
                result.set("diskTotal", optionalNumber(info.disk_total));
                result.set("memoryFree", optionalNumber(info.memory_free));
                result.set("memoryTotal", optionalNumber(info.memory_total));
                result.set("memoryUsed", static_cast<double>(info.memory_used));
                result.set("memoryMax", static_cast<double>(info.memory_max));
                result.set("memoryMaxTime", info.memory_max_time);
                result.set("threads", info.threads);
                result.set("threadsMax", info.threads_max);
                return result;
            }

        private:
            common::StdFileSystem             fs;
            common::SystemClock               clock;
            downloader::FetchTransport        http;
            std::string                       root;
            PlayerRuntimeConfig               config;
            std::unique_ptr<PlayerRuntimeApi> runtime;
            NewLayout                         layout; // the document is kept until the next one

            val toJs(const CoreEvent &event)
            {
                return std::visit(
                    Overloaded{
                        [this](const smil::StartMedia &e) { return startMedia(e); },
                        [](const smil::StopMedia &e) { return command("stop", e.id); },
                        [](const smil::RepeatMedia &e) { return command("repeat", e.id); },
                        [](const smil::PauseMedia &e) { return command("pause", e.id); },
                        [](const smil::ResumeMedia &e) { return command("resume", e.id); },
                        [this](const NewLayout &e) { return newLayout(e); },
                        [](const BackgroundImage &e)
                        {
                            val result = val::object();
                            result.set("type", std::string("background"));
                            result.set("src", e.src);
                            result.set("path", e.path);
                            return result;
                        },
                        [](const tasks::ClearCaches &)
                        {
                            val result = val::object();
                            result.set("type", std::string("clearCaches"));
                            return result;
                        },
                        [](const Screenshot &e)
                        {
                            val result = val::object();
                            result.set("type", std::string("screenshot"));
                            result.set("path", e.path);
                            return result;
                        },
                    },
                    event);
            }

            static val command(const char *type, smil::InstanceId id)
            {
                val result = val::object();
                result.set("type", std::string(type));
                result.set("id", id);
                return result;
            }

            std::string background(const std::string &src) const
            {
                auto found = layout.backgrounds.find(src);
                return found == layout.backgrounds.end() ? std::string() : found->second;
            }

            val newLayout(const NewLayout &next)
            {
                layout                 = next;
                const smil::Head &head = layout.document->head;

                val regions = val::array();
                for (const smil::Region &region : head.regions)
                {
                    val entry = val::object();
                    entry.set("name", region.region_name.empty() ? region.id : region.region_name);
                    entry.set("left", region.left);
                    entry.set("top", region.top);
                    entry.set("width", region.width);
                    entry.set("height", region.height);
                    entry.set("color", region.background_color);
                    entry.set("imageSrc", region.background_image);
                    entry.set("image", background(region.background_image));
                    entry.set("repeat", lower(region.background_repeat));
                    regions.call<void>("push", entry);
                }

                val result = val::object();
                result.set("type", std::string("layout"));
                result.set("title", head.title);
                result.set("rootColor", head.root_layout.background_color);
                result.set("rootImageSrc", head.root_layout.background_image);
                result.set("rootImage", background(head.root_layout.background_image));
                result.set("rootRepeat", lower(head.root_layout.background_repeat));
                result.set("regions", regions);
                return result;
            }

            /// fit, mediaAlign and soundLevel of the medium, else of the region: per region of the instance
            val startMedia(const smil::StartMedia &command)
            {
                const smil::Media &media = *command.media;
                std::string        kind  = kindName(media.kind);
                if (kind.empty() || !layout.document)
                    return val::null();

                const std::vector<smil::Region> &definitions = layout.document->head.regions;
                val                              regions     = val::array();
                for (std::size_t index : command.regions)
                {
                    if (index >= definitions.size())
                        continue;
                    const smil::Region &region = definitions[index];
                    val                 entry  = val::object();
                    entry.set("index", static_cast<int>(index));
                    entry.set("fit", lower(media.fit.empty() ? region.fit : media.fit));
                    entry.set("align", lower(media.media_align.empty() ? region.media_align : media.media_align));
                    entry.set("volume", volumeOf(media.sound_level.empty() ? region.sound_level : media.sound_level));
                    regions.call<void>("push", entry);
                }

                val result = val::object();
                result.set("type", std::string("start"));
                result.set("id", command.id);
                result.set("kind", kind);
                result.set("source", command.source);
                result.set("color", media.color);
                result.set("label", media.title.empty() ? media.src : media.title);
                result.set("regions", regions);
                return result;
            }
    };
}

EMSCRIPTEN_BINDINGS(garlic_player)
{
    emscripten::class_<player::Player>("Player")
        .constructor<emscripten::val>()
        .function("start", &player::Player::start)
        .function("poll", &player::Player::poll)
        .function("msUntilNext", &player::Player::msUntilNext)
        .function("indexUri", &player::Player::indexUri)
        .function("mediaFinished", &player::Player::mediaFinished)
        .function("mediaFailed", &player::Player::mediaFailed)
        .function("activated", &player::Player::activated)
        .function("accessKey", &player::Player::accessKey)
        .function("screenshotTaken", &player::Player::screenshotTaken)
        .function("cachesCleared", &player::Player::cachesCleared)
        .function("stop", &player::Player::stop)
        .function("settings", &player::Player::settings)
        .function("changeSettings", &player::Player::changeSettings)
        .function("debugInfo", &player::Player::debugInfo);
}
