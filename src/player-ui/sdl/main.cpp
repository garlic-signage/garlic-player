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
#include "paths.hpp"
#include "player_window.hpp"
#include "supervisor_client.hpp"
#include "web_view.hpp"
#ifdef PLAYER_SDL_WITH_MPV
    #include "mpv_media.hpp"
#endif

#include "common/clock.hpp"
#include "common/std_file_system.hpp"
#include "configuration/configuration.hpp"
#include "downloader/curl_transport.hpp"
#include "player_runtime/threaded_runtime.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <charconv>
#include <filesystem>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace
{
    constexpr const char *usage = R"(Usage: garlic-player-sdl [options] [SMIL_INDEX]

  SMIL_INDEX                    URI or path of the SMIL index; empty: the one in the settings

  -c, --config <config>         path to a config.xml, applied at every start (e.g. by a launcher)
  -m, --windows-mode <mode>     fullscreen, bigscreen (over all screens) or windowed (default)
  -z, --windows-size <size>     size in windows mode, e.g. 980x540 (default)
  -s, --screen-select <screen>  number of the screen to start on (default 0)
      --supervisor <endpoint>   endpoint of the supervisor (garlic-daemon: Unix socket)
      --video-quality <quality> auto (default: low on a Raspberry Pi, otherwise normal), low, normal or high
  -h, --help                    this help
  -v, --version                 the version
)";

    /// The options of the qwidget player: "-m windowed", "--windows-mode windowed", "--windows-mode=windowed"
    struct Options
    {
            std::map<std::string, std::string> values;
            std::vector<std::string>           positional;
            bool                               help    = false;
            bool                               version = false;
    };

    std::optional<Options> parse(int argc, char *argv[])
    {
        const std::map<std::string, std::string> names{
            {"-c", "config"},        {"--config", "config"},          {"-m", "windows-mode"},
            {"--windows-mode", "windows-mode"}, {"-z", "windows-size"}, {"--windows-size", "windows-size"},
            {"-s", "screen-select"}, {"--screen-select", "screen-select"}, {"--supervisor", "supervisor"},
            {"--video-quality", "video-quality"},
        };
        Options options;
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            if (arg == "-h" || arg == "--help")
                options.help = true;
            else if (arg == "-v" || arg == "--version")
                options.version = true;
            else if (arg.starts_with('-'))
            {
                std::string value;
                bool        inline_value = false;
                if (auto equal = arg.find('='); equal != std::string::npos)
                {
                    value        = arg.substr(equal + 1);
                    arg          = arg.substr(0, equal);
                    inline_value = true;
                }
                auto name = names.find(arg);
                if (name == names.end())
                {
                    std::cerr << "unknown option " << arg << "\n";
                    return std::nullopt;
                }
                if (!inline_value)
                {
                    if (i + 1 >= argc)
                    {
                        std::cerr << "missing value for " << arg << "\n";
                        return std::nullopt;
                    }
                    value = argv[++i];
                }
                options.values[name->second] = value;
            }
            else
                options.positional.push_back(arg);
        }
        return options;
    }

    std::string value(const Options &options, const std::string &name, const std::string &fallback = {})
    {
        auto found = options.values.find(name);
        return found == options.values.end() ? fallback : found->second;
    }

    int number(const std::string &text, int fallback)
    {
        int result        = 0;
        auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
        return error == std::errc{} ? result : fallback;
    }

    /// The places of the Qt players and garlic-lib, so an existing player keeps its settings, cache and logs.
    garlic::core::PlayerRuntimeConfig runtimeConfig(const std::string &index_uri, const std::string &configuration_file)
    {
        player::Paths                     paths = player::standardPaths();
        garlic::core::PlayerRuntimeConfig config;
        config.ini_path  = paths.ini_file;
        config.cache_dir = paths.cache_dir;
        config.log_dir   = paths.log_dir;
        config.version   = PLAYER_VERSION;
        config.index_uri = index_uri;
        if (!configuration_file.empty())
            config.configuration_file = std::filesystem::absolute(configuration_file).string();
        config.random_seed = std::random_device{}();
        return config;
    }

    /// The font of the menu and the dialogs, copied next to the program at build time
    std::string fontFile()
    {
        const char *base = SDL_GetBasePath();
        return std::string(base != nullptr ? base : "") + "fonts/DejaVuSans.ttf";
    }
}

int main(int argc, char *argv[])
{
    // the helper processes of the web engine (renderer, GPU …) start this program again: they end here
    if (int helper = player::runWebHelperProcess(argc, argv); helper >= 0)
        return helper;

    auto options = parse(argc, argv);
    if (!options)
    {
        std::cerr << usage;
        return 1;
    }
    if (options->help)
    {
        std::cout << usage;
        return 0;
    }
    if (options->version)
    {
        std::cout << "garlic-player " << PLAYER_VERSION << "\n";
        return 0;
    }

    std::string index_uri;
    if (!options->positional.empty())
    {
        auto valid = garlic::core::configuration::Configuration::validateIndexUri(options->positional.front());
        if (!valid)
        {
            std::cerr << valid.error() << "\n";
            return 1;
        }
        index_uri = *valid;
    }

    player::WindowMode mode = player::WindowMode::Windowed;
    std::string        mode_name = value(*options, "windows-mode", "windowed");
    if (mode_name == "fullscreen")
        mode = player::WindowMode::Fullscreen;
    else if (mode_name == "bigscreen")
        mode = player::WindowMode::Bigscreen;
    std::string size   = value(*options, "windows-size", "980x540");
    int         width  = number(size.substr(0, size.find('x')), 0);
    int         height = size.find('x') == std::string::npos ? 0 : number(size.substr(size.find('x') + 1), 0);
    if (width <= 0 || height <= 0)
    {
        width  = 980;
        height = 540;
    }
    std::string quality = value(*options, "video-quality", "auto");
    if (quality != "auto" && quality != "low" && quality != "normal" && quality != "high")
    {
        std::cerr << "unknown video quality: " << quality << "\n" << usage;
        return 1;
    }
#ifdef PLAYER_SDL_WITH_MPV
    player::setVideoQuality(quality == "low"      ? player::VideoQuality::Low
                            : quality == "normal" ? player::VideoQuality::Normal
                            : quality == "high"   ? player::VideoQuality::High
                                                  : player::VideoQuality::Auto);
#endif

    player::prepareWebViews();
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0"); // touches come as touches, not a second time as mouse clicks
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "0");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) || !TTF_Init())
    {
        std::cerr << "garlic-player cannot start SDL: " << SDL_GetError() << "\n";
        return 1;
    }
    std::uint32_t wake_event = SDL_RegisterEvents(1);

    int exit_code = 1;
    {
        // supervisor protocol (src/core/docs/supervisor_protocol.md): hello before the core starts, its uuid is a
        // start value; without answer the player starts anyway and the client keeps trying
        auto                                      config = runtimeConfig(index_uri, value(*options, "config"));
        std::unique_ptr<player::SupervisorClient> supervisor;
        if (options->values.contains("supervisor"))
        {
            supervisor = std::make_unique<player::SupervisorClient>(value(*options, "supervisor"), PLAYER_VERSION);
            supervisor->start(std::chrono::seconds(3));
            config.supervisor = supervisor.get();
            supervisor->seed(config);
        }

        garlic::core::common::StdFileSystem     fs;
        garlic::core::common::SystemClock       clock;
        garlic::core::downloader::CurlTransport http;
        garlic::core::ThreadedRuntime           runtime(fs, clock, http, config,
                                                        [wake_event]
                                                        {
                                                            // core thread: into the UI thread
                                                            SDL_Event wake{};
                                                            wake.type = wake_event;
                                                            SDL_PushEvent(&wake);
                                                        });

        player::PlayerWindow window(runtime, wake_event);
        if (auto opened = window.open(mode, width, height, number(value(*options, "screen-select", "0"), 0), fontFile());
            !opened)
        {
            std::cerr << "garlic-player cannot start: " << opened.error() << "\n";
            TTF_Quit();
            SDL_Quit();
            return 1;
        }
        if (auto started = runtime.start(); !started)
        {
            std::cerr << "garlic-player cannot start: " << started.error() << "\n";
            TTF_Quit();
            SDL_Quit();
            return 1;
        }

        // without an index nothing can be played: the settings dialog first, cancelled ends the player (garlic-lib);
        // not when the supervisor gives the content URL or has locked the player
        std::promise<std::string> stored_uri;
        runtime.post(
            [&](garlic::core::PlayerRuntimeApi &r)
            {
                if (supervisor && supervisor->ready())
                    player::SupervisorClient::applyStartValues(r, supervisor->hello());
                stored_uri.set_value(r.configuration().indexUri());
            });
        bool without_index = stored_uri.get_future().get().empty();
        bool configures    = supervisor && supervisor->offers("configuration");
        bool locked        = supervisor && supervisor->locked();

        if (supervisor)
        {
            window.setIdentityLocked(configures);
            window.setLocked(locked);
            window.setScreenControl(supervisor.get());
            window.setDeviceControl(supervisor.get());
            supervisor->setCoreProbe([&runtime](std::function<void()> done)
                                     { runtime.post([done](garlic::core::PlayerRuntimeApi &) { done(); }); });
            supervisor->setHandlers({
                .locked_changed          = [&window](bool now_locked) { window.setLocked(now_locked); },
                .configuration_requested = [&window](const std::string &path) { window.applyConfiguration(path); },
                .index_requested         = [&window](const std::string &uri) { window.playIndex(uri); },
                .quit_requested =
                    [&]
                    {
                        supervisor->closing("quit");
                        window.quitFromSupervisor();
                    },
            });
            window.setPoll([&supervisor] { supervisor->poll(); });
            window.setOnQuit([&supervisor] { supervisor->closing("user"); });
        }
        if (without_index && !configures && !locked)
            window.requireSettings();

        exit_code = window.run();
        runtime.stop(); // before the window goes: no wake may reach it any more
    }
    player::shutdownWebViews(); // after the last web page went with the window
    TTF_Quit();
    SDL_Quit();
    return exit_code;
}
