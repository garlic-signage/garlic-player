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
#ifndef PLAYER_UI_SDL_PLAYER_WINDOW_HPP
#define PLAYER_UI_SDL_PLAYER_WINDOW_HPP

#include "blank_screen.hpp"
#include "dialogs.hpp"
#include "media.hpp"
#include "ui/controls.hpp"
#include "ui/sdl_canvas.hpp"

#include "player_runtime/threaded_runtime.hpp"

#include <SDL3/SDL.h>

#include <chrono>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace player
{
    enum class WindowMode
    {
        Windowed,
        Fullscreen,
        Bigscreen // one borderless window over all screens (garlic-lib)
    };

    /**
     * The window of the player: takes the events of the core, keeps the regions of
     * a NewLayout and draws the media in them with one SDL renderer, z-index from bottom to top. What the frontend
     * learns (end of a video, a click, a key) goes back to the core by post().
     *
     * The main loop is run(): SDL events, the wake of the ThreadedRuntime (an SDL user event), timers, the supervisor,
     * and drawing when something changed.
     */
    class PlayerWindow
    {
        public:
            PlayerWindow(garlic::core::ThreadedRuntime &runtime, std::uint32_t wake_event);
            ~PlayerWindow();

            PlayerWindow(const PlayerWindow &)            = delete;
            PlayerWindow &operator=(const PlayerWindow &) = delete;

            /// Opens the window and the renderer; font_file is the font of the menu and the dialogs
            std::expected<void, std::string> open(WindowMode mode, int width, int height, int screen,
                                                  const std::string &font_file);

            /// Runs until the player ends; the exit code
            int run();

            /// Without an index: the settings dialog first, cancelled ends the player with 1 (garlic-lib)
            void requireSettings() { openSettingsDialog(true); }

            /// Called every loop turn, e.g. SupervisorClient::poll(); the loop then wakes at least every 100 ms
            void setPoll(std::function<void()> poll) { extra_poll = std::move(poll); }

            /// Before the player ends on purpose (menu, Ctrl+Q, debug dialog): e.g. closing("user")
            void setOnQuit(std::function<void()> on_quit) { quit_hook = std::move(on_quit); }

            void setScreenControl(ScreenControl *control)
            {
                screen_control = control;
                blank_screen.setControl(control);
            }
            void setDeviceControl(DeviceControl *control) { device_control = control; }
            void setIdentityLocked(bool locked) { identity_locked = locked; }

            /// Locked by the supervisor: no menu, no dialogs, no shortcuts; accesskeys of the SMIL stay. Closes what
            /// is open.
            void setLocked(bool locked);

            /// From the supervisor: a configuration.xml to apply, a SMIL index to play from now on, the end
            void applyConfiguration(const std::string &path);
            void playIndex(const std::string &uri);
            void quitFromSupervisor() { running = false; }

        private:
            /// One <region> of the layout: its definition and its media, the newest on top
            struct Region
            {
                    const garlic::core::smil::Region *definition = nullptr;
                    std::vector<Media *>              stack;
                    SDL_Texture                      *background = nullptr;
            };

            /// One StartMedia: the medium in every region with its name
            struct Instance
            {
                    std::vector<std::pair<std::size_t, std::unique_ptr<Media>>> shown; // region index, medium
            };

            struct Deferred
            {
                    garlic::core::smil::InstanceId id;
                    Outcome                        outcome;
                    std::string                    message;
            };

            using Clock = std::chrono::steady_clock;

            garlic::core::ThreadedRuntime &runtime;
            std::uint32_t                  wake_event;
            SDL_Window                    *window   = nullptr;
            SDL_Renderer                  *renderer = nullptr;
            std::unique_ptr<ui::SdlCanvas> canvas;
            WindowMode                     mode = WindowMode::Windowed;
            int                            windowed_width  = 980;
            int                            windowed_height = 540;

            garlic::core::NewLayout                                layout; // the document is kept until the next one
            std::vector<Region>                                    regions; // by z-index, bottom first
            SDL_Texture                                           *root_background = nullptr;
            std::map<garlic::core::smil::InstanceId, Instance>     instances;
            std::vector<Deferred>                                  deferred;
            std::optional<std::string>                             screenshot_path;
            garlic::core::Downloads                                downloads; // "Loading" while no medium is shown
            bool media_shown = false; // since the layout: "Loading" only before its first medium, not in gaps later
            Media                                                 *web_focus = nullptr; // the web page that has the keyboard

            // ten fast clicks/touches or one held for 10 s open the menu (also for the review of the App Store)
            std::optional<Clock::time_point> hold_until;
            Clock::time_point                released_at{};
            int                              fast_clicks = 0;
            ui::Point                        menu_position;

            ui::ContextMenu         menu;
            std::unique_ptr<Dialog> dialog;
            bool                    settings_required = false;
            Clock::time_point       next_tick{};

            BlankScreen            blank_screen;
            ScreenControl         *screen_control = nullptr; // the supervisor: REST hardware/display
            DeviceControl         *device_control  = nullptr;
            bool                   identity_locked = false;
            bool                   is_locked       = false;
            bool                   running         = true;
            int                    exit_code       = 0;
            bool                   dirty           = true;
            std::function<void()>  extra_poll;
            std::function<void()>  quit_hook;

            void handle(const SDL_Event &event);
            void takeEvents();
            void handleCore(const garlic::core::CoreEvent &event);
            void drawLoading();
            void newLayout(const garlic::core::NewLayout &next);
            void backgroundImage(const garlic::core::BackgroundImage &image);
            void start(const garlic::core::smil::StartMedia &command);
            void stop(garlic::core::smil::InstanceId id);
            void forEach(garlic::core::smil::InstanceId id, void (Media::*action)());
            void raise(garlic::core::smil::InstanceId id);
            void blankScreen(garlic::core::smil::InstanceId id);
            void clearCaches(const garlic::core::tasks::ClearCaches &clear);
            void clear();
            void report();
            void tellDisplayMode();

            void key(const SDL_KeyboardEvent &event);
            void pointer(ui::PointerEvent event, bool is_mouse);
            void menuGesture(bool press, ui::Point position);
            void regionClicked(ui::Point position);
            void forwardMouse(const SDL_Event &event, float x, float y);
            void setWebFocus(Media *media);

            void openMenu();
            void openSettingsDialog(bool required = false);
            void openDebugDialog();
            void closeDialog();
            void quit();

            void setMode(WindowMode next, int screen = -1);
            void updateCursor();

            void                       render();
            void                       drawBackground(SDL_Texture *texture, const std::string &color,
                                                      const std::string &repeat, const SDL_FRect &area);
            [[nodiscard]] SDL_FRect    regionArea(const garlic::core::smil::Region &definition) const;
            [[nodiscard]] ui::Rect     windowPoints() const;
            [[nodiscard]] float        uiScale() const;
            [[nodiscard]] std::vector<std::string> playingMedia() const;
            [[nodiscard]] SDL_Texture *loadBackground(const std::string &src);
            void                       saveScreenshot(const std::string &path);
    };
}

#endif // PLAYER_UI_SDL_PLAYER_WINDOW_HPP
