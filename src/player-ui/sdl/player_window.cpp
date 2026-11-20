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
#include "player_window.hpp"

#include "color.hpp"
#include "ui/look.hpp"
#include "web_view.hpp"
#ifdef PLAYER_SDL_WITH_MPV
    #include "mpv_media.hpp"
#endif

#include "configuration/configuration.hpp"

#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cctype>
#include <iostream>

namespace player
{
    using namespace garlic::core;

    namespace
    {
        template <typename... Functions> struct Overloaded : Functions...
        {
                using Functions::operator()...;
        };

        constexpr auto fast_click_gap = std::chrono::milliseconds(500);
        constexpr auto hold_time      = std::chrono::seconds(10);

        enum MenuId
        {
            menu_settings,
            menu_debug,
            menu_exit
        };

        /// The user event of the wake: code 0 new core events, code 1 a medium has a new frame
        constexpr Sint32 wake_core   = 0;
        constexpr Sint32 wake_redraw = 1;

        bool equalsIgnoringCase(std::string_view a, std::string_view b)
        {
            return std::ranges::equal(a, b, [](unsigned char x, unsigned char y)
                                      { return std::tolower(x) == std::tolower(y); });
        }

        bool isUrl(const std::string &path)
        {
            return path.find("://") != std::string::npos;
        }
    }

    PlayerWindow::PlayerWindow(ThreadedRuntime &r, std::uint32_t wake) : runtime(r), wake_event(wake)
    {
        menu.setItems({{menu_settings, "Settings", "Ctrl+S"}, {menu_debug, "Debug infos", "Ctrl+D"},
                       {menu_exit, "Exit", "Ctrl+Q"}});
    }

    PlayerWindow::~PlayerWindow()
    {
        clear(); // the media before the regions and the renderer
        for (Region &region : regions)
            if (region.background != nullptr)
                SDL_DestroyTexture(region.background);
        if (root_background != nullptr)
            SDL_DestroyTexture(root_background);
        canvas.reset();
#ifdef PLAYER_SDL_WITH_MPV
        shutdownMpv();
#endif
        if (renderer != nullptr)
            SDL_DestroyRenderer(renderer);
        if (window != nullptr)
            SDL_DestroyWindow(window);
    }

    std::expected<void, std::string> PlayerWindow::open(WindowMode start_mode, int width, int height, int screen,
                                                        const std::string &font_file)
    {
        windowed_width  = width;
        windowed_height = height;
        window = SDL_CreateWindow("garlic-player", width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (window == nullptr)
            return std::unexpected(std::string("cannot open the window: ") + SDL_GetError());
        // OpenGL (OpenGL ES on the Raspberry Pi): libmpv renders in a context that shares textures with this one
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl,opengles2");
        renderer = SDL_CreateRenderer(window, nullptr);
        if (renderer == nullptr)
            return std::unexpected(std::string("cannot create the renderer: ") + SDL_GetError());
        SDL_SetRenderVSync(renderer, 1);
        auto opened = ui::SdlCanvas::open(*renderer, font_file);
        if (!opened)
            return std::unexpected(opened.error());
        canvas = std::move(*opened);
        SDL_ShowWindow(window);
        setMode(start_mode, screen);
        return {};
    }

    int PlayerWindow::run()
    {
        while (running)
        {
            auto now     = Clock::now();
            auto timeout = std::chrono::milliseconds(1000);
            if (extra_poll)
                timeout = std::chrono::milliseconds(100);
            if (webViewsRunning())
                timeout = std::min(timeout, std::chrono::milliseconds(33));
            auto until = [&](Clock::time_point when)
            {
                auto left = std::chrono::duration_cast<std::chrono::milliseconds>(when - now);
                timeout   = std::clamp(left, std::chrono::milliseconds(0), timeout);
            };
            if (hold_until)
                until(*hold_until);
            if (dialog)
                until(next_tick);

            SDL_Event event;
            if (!dirty && SDL_WaitEventTimeout(&event, static_cast<Sint32>(timeout.count())))
                handle(event);
            while (running && SDL_PollEvent(&event))
                handle(event);
            if (!running)
                break;

            if (extra_poll)
                extra_poll();
            pumpWebViews();
            for (auto &[id, instance] : instances)
                for (auto &[index, media] : instance.shown)
                    media->update();
            report();

            now = Clock::now();
            if (hold_until && now >= *hold_until)
            {
                hold_until.reset();
                openMenu();
            }
            if (dialog && now >= next_tick)
            {
                next_tick = now + std::chrono::seconds(1);
                dialog->tick();
                dirty = true;
            }
            if (dirty)
                render();
        }
        return exit_code;
    }

    // ------------------------------------------------------------------ SDL events

    void PlayerWindow::handle(const SDL_Event &event)
    {
        if (event.type == wake_event)
        {
            if (event.user.code == wake_core)
                takeEvents();
            dirty = true;
            return;
        }
        switch (event.type)
        {
            case SDL_EVENT_QUIT: running = false; break; // the window was closed: no closing()
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_EXPOSED:
            case SDL_EVENT_WINDOW_DISPLAY_CHANGED: dirty = true; break;
            case SDL_EVENT_KEY_DOWN: key(event.key); break;
            case SDL_EVENT_TEXT_INPUT:
                if (dialog && dialog->wantsText())
                {
                    dialog->input(std::string(event.text.text));
                    dirty = true;
                }
                else if (web_focus != nullptr && !dialog && !menu.isOpen())
                    web_focus->input(event, 0, 0);
                break;
            case SDL_EVENT_KEY_UP:
                if (web_focus != nullptr && !dialog && !menu.isOpen())
                    web_focus->input(event, 0, 0);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.which != SDL_TOUCH_MOUSEID)
                    pointer({{0, 0},
                             event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? ui::PointerPhase::Down : ui::PointerPhase::Up,
                             {event.button.x, event.button.y}},
                            true);
                if (event.button.which != SDL_TOUCH_MOUSEID)
                    forwardMouse(event, event.button.x, event.button.y);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (event.motion.which != SDL_TOUCH_MOUSEID && event.motion.state != 0)
                    pointer({{0, 0}, ui::PointerPhase::Move, {event.motion.x, event.motion.y}}, true);
                if (event.motion.which != SDL_TOUCH_MOUSEID)
                    forwardMouse(event, event.motion.x, event.motion.y);
                break;
            case SDL_EVENT_MOUSE_WHEEL: forwardMouse(event, event.wheel.mouse_x, event.wheel.mouse_y); break;
            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_MOTION:
            {
                int width  = 0;
                int height = 0;
                SDL_GetWindowSize(window, &width, &height);
                ui::PointerPhase phase = event.type == SDL_EVENT_FINGER_DOWN ? ui::PointerPhase::Down
                                         : event.type == SDL_EVENT_FINGER_UP ? ui::PointerPhase::Up
                                                                             : ui::PointerPhase::Move;
                pointer({{event.tfinger.touchID, event.tfinger.fingerID},
                         phase,
                         {event.tfinger.x * static_cast<float>(width), event.tfinger.y * static_cast<float>(height)}},
                        false);
                break;
            }
            default: break;
        }
    }

    /// Ctrl+F full screen, Ctrl+B over all screens, Ctrl+S settings, Ctrl+D debug infos, Ctrl+Q quit (garlic-lib);
    /// other keys are accessKeys. While a dialog is open, the keys go to it.
    void PlayerWindow::key(const SDL_KeyboardEvent &event)
    {
        if (dialog)
        {
            bool shift = (event.mod & SDL_KMOD_SHIFT) != 0;
            bool ctrl  = (event.mod & SDL_KMOD_CTRL) != 0;
            using ui::EditKey;
            // like the line edits of the desktops: arrows, Home, End, with Shift selecting and with Ctrl by words;
            // Ctrl+A all, Ctrl+C / Ctrl+X / Ctrl+V the clipboard (pasting is typing)
            auto pick = [&](EditKey plain, EditKey selecting) { dialog->input(shift ? selecting : plain); };
            switch (event.key)
            {
                case SDLK_BACKSPACE: dialog->input(EditKey::Backspace); break;
                case SDLK_DELETE: dialog->input(EditKey::Delete); break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER: dialog->input(EditKey::Enter); break;
                case SDLK_ESCAPE: dialog->input(EditKey::Escape); break;
                case SDLK_TAB: dialog->input(shift ? EditKey::BackTab : EditKey::Tab); break;
                case SDLK_LEFT:
                    ctrl ? pick(EditKey::WordLeft, EditKey::SelectWordLeft) : pick(EditKey::Left, EditKey::SelectLeft);
                    break;
                case SDLK_RIGHT:
                    ctrl ? pick(EditKey::WordRight, EditKey::SelectWordRight)
                         : pick(EditKey::Right, EditKey::SelectRight);
                    break;
                case SDLK_HOME: pick(EditKey::Home, EditKey::SelectHome); break;
                case SDLK_END: pick(EditKey::End, EditKey::SelectEnd); break;
                case SDLK_A:
                    if (ctrl)
                        dialog->input(EditKey::SelectAll);
                    break;
                case SDLK_C:
                case SDLK_X:
                    if (ctrl)
                        if (std::string selected = dialog->selectedText(); !selected.empty())
                        {
                            SDL_SetClipboardText(selected.c_str());
                            if (event.key == SDLK_X)
                                dialog->input(EditKey::Delete);
                        }
                    break;
                case SDLK_V:
                    if (ctrl && SDL_HasClipboardText())
                        if (char *pasted = SDL_GetClipboardText(); pasted != nullptr)
                        {
                            dialog->input(std::string(pasted));
                            SDL_free(pasted);
                        }
                    break;
                default: break;
            }
            dirty = true;
            closeDialog();
            return;
        }
        if (menu.isOpen())
        {
            if (event.key == SDLK_ESCAPE)
            {
                menu.close();
                updateCursor();
                dirty = true;
            }
            return;
        }

        bool ctrl = (event.mod & SDL_KMOD_CTRL) != 0;
        if (ctrl && !is_locked)
        {
            switch (event.key)
            {
                case SDLK_F: setMode(mode == WindowMode::Fullscreen ? WindowMode::Windowed : WindowMode::Fullscreen); return;
                case SDLK_B: setMode(WindowMode::Bigscreen); return;
                case SDLK_D: openDebugDialog(); return;
                case SDLK_S: openSettingsDialog(); return;
                case SDLK_Q: quit(); return;
                default: break;
            }
        }
        if (web_focus != nullptr) // the page has the focus: the keys are its own, no access keys
        {
            SDL_Event forwarded{};
            forwarded.key = event;
            web_focus->input(forwarded, 0, 0);
            return;
        }
        // like event->text() of Qt without modifiers: the character of the key in the keyboard layout
        bool modifiers = (event.mod & (SDL_KMOD_CTRL | SDL_KMOD_SHIFT | SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0;
        if (!modifiers && event.key >= 0x20 && event.key < 0x7F && !event.repeat)
        {
            char key = static_cast<char>(std::tolower(static_cast<int>(event.key)));
            runtime.post([key](PlayerRuntimeApi &r) { r.accessKey(key); });
        }
    }

    void PlayerWindow::pointer(ui::PointerEvent event, bool is_mouse)
    {
        (void)is_mouse;
        // window coordinates → pixels → points of the menu and the dialogs
        float pixels       = SDL_GetWindowPixelDensity(window);
        ui::Point in_pixels{event.position.x * pixels, event.position.y * pixels};
        float     scale    = uiScale();
        event.position     = {in_pixels.x / scale, in_pixels.y / scale};
        dirty              = true;

        if (menu.isOpen())
        {
            if (event.phase == ui::PointerPhase::Down)
                menu.pointerDown(event.position);
            else if (event.phase == ui::PointerPhase::Up)
                menu.pointerUp(event.position);
            if (!menu.isOpen())
            {
                updateCursor();
                switch (menu.takeChosen().value_or(-1))
                {
                    case menu_settings: openSettingsDialog(); break;
                    case menu_debug: openDebugDialog(); break;
                    case menu_exit: quit(); break;
                    default: break;
                }
            }
            return;
        }
        if (dialog)
        {
            dialog->pointer(event);
            closeDialog();
            return;
        }
        if (event.phase == ui::PointerPhase::Move)
            return;
        bool press = event.phase == ui::PointerPhase::Down;
        if (!is_locked)
            menuGesture(press, event.position);
        if (!press)
            regionClicked(in_pixels);
    }

    /**
     * Ten fast clicks or touches (less than 500 ms apart) or one held for 10 s anywhere in the window open the menu
     * with settings, debug infos and exit; the same in every frontend.
     */
    void PlayerWindow::menuGesture(bool press, ui::Point position)
    {
        auto now = Clock::now();
        if (press)
        {
            fast_clicks   = now - released_at < fast_click_gap ? fast_clicks + 1 : 1;
            menu_position = position;
            if (fast_clicks >= 10)
            {
                fast_clicks = 0;
                hold_until.reset();
                openMenu();
            }
            else
                hold_until = now + hold_time; // opens the menu while it is still held
        }
        else
        {
            released_at = now;
            hold_until.reset();
        }
    }

    /// A click on a region activates the medium on top of it (clicks go through transparent parts in no Qt player
    /// either: the topmost region under the pointer takes it).
    void PlayerWindow::regionClicked(ui::Point position)
    {
        for (auto region = regions.rbegin(); region != regions.rend(); ++region)
        {
            SDL_FRect area = regionArea(*region->definition);
            if (position.x < area.x || position.y < area.y || position.x >= area.x + area.w || position.y >= area.y + area.h)
                continue;
            if (region->stack.empty())
                return;
            Media *top = region->stack.back();
            if (top->interactive()) // web pages take their clicks themselves (like the Qt players)
                return;
            for (auto &[id, instance] : instances)
                for (auto &[index, media] : instance.shown)
                    if (media.get() == top)
                    {
                        smil::InstanceId activated = id;
                        runtime.post([activated](PlayerRuntimeApi &r) { r.elementActivated(activated); });
                        return;
                    }
            return;
        }
    }

    // ------------------------------------------------------------------ core events

    void PlayerWindow::takeEvents()
    {
        for (const CoreEvent &event : runtime.takeEvents())
            handleCore(event);
    }

    void PlayerWindow::handleCore(const CoreEvent &event)
    {
        std::visit(Overloaded{
                       [this](const smil::StartMedia &e) { start(e); },
                       [this](const smil::StopMedia &e) { stop(e.id); },
                       [this](const smil::RepeatMedia &e) { forEach(e.id, &Media::repeat); },
                       [this](const smil::PauseMedia &e) { forEach(e.id, &Media::pause); },
                       [this](const smil::ResumeMedia &e)
                       {
                           forEach(e.id, &Media::resume);
                           raise(e.id);
                       },
                       [this](const NewLayout &e) { newLayout(e); },
                       [this](const BackgroundImage &e) { backgroundImage(e); },
                       [this](const tasks::ClearCaches &e) { clearCaches(e); },
                       [this](const Screenshot &e) { screenshot_path = e.path; },
                       [this](const DeviceSettings &e)
                       {
                           if (device_control != nullptr)
                               device_control->deviceSettings(e);
                       },
                       [this](const Downloads &e) { downloads = e; },
                   },
                   event);
        dirty = true;
    }

    /// A new index: the old media are stopped already (StopMedia come first), so only the regions go.
    void PlayerWindow::newLayout(const NewLayout &next)
    {
        clear();
        media_shown = false; // a new playlist may wait for its files again
        for (Region &region : regions)
            if (region.background != nullptr)
                SDL_DestroyTexture(region.background);
        regions.clear();
        if (root_background != nullptr)
            SDL_DestroyTexture(root_background);
        layout          = next;
        const smil::Head &head = layout.document->head;
        root_background = loadBackground(head.root_layout.background_image);
        for (const smil::Region &definition : head.regions) // sorted by z-index: a later one lies above
            regions.push_back({&definition, {}, loadBackground(definition.background_image)});
    }

    SDL_Texture *PlayerWindow::loadBackground(const std::string &src)
    {
        auto found = layout.backgrounds.find(src);
        if (found == layout.backgrounds.end() || isUrl(found->second))
            return nullptr;
        return IMG_LoadTexture(renderer, found->second.c_str());
    }

    void PlayerWindow::backgroundImage(const BackgroundImage &image)
    {
        if (!layout.document)
            return;
        auto load = [&](SDL_Texture *&texture)
        {
            if (texture != nullptr)
                SDL_DestroyTexture(texture);
            texture = isUrl(image.path) ? nullptr : IMG_LoadTexture(renderer, image.path.c_str());
        };
        if (layout.document->head.root_layout.background_image == image.src)
            load(root_background);
        for (Region &region : regions)
            if (region.definition->background_image == image.src)
                load(region.background);
    }

    void PlayerWindow::start(const smil::StartMedia &command)
    {
        stop(command.id); // the same instance again: start anew
        if (command.media->kind == smil::MediaKind::Command && equalsIgnoringCase(command.source, "adapi:blankScreen"))
        {
            blankScreen(command.id);
            return;
        }
        // no pointer to the window: the web engine may still call it after the window went
        MediaContext context{renderer, [wake = wake_event]
                             {
                                 SDL_Event redraw{};
                                 redraw.type      = wake;
                                 redraw.user.code = wake_redraw;
                                 SDL_PushEvent(&redraw); // from any thread
                             }};
        Instance &instance = instances[command.id];
        for (std::size_t index : command.regions)
        {
            if (index >= regions.size())
                continue;
            // only the first region reports: the core expects one end per instance
            Report report = [](Outcome, const std::string &) {};
            if (instance.shown.empty())
                report = [this, id = command.id](Outcome outcome, const std::string &message)
                { deferred.push_back({id, outcome, message}); };
            auto media = createMedia(*command.media, *regions[index].definition, command.source, std::move(report),
                                     context);
            if (!media)
                continue;
            if (media->visible())
                regions[index].stack.push_back(media.get());
            media->start();
            instance.shown.emplace_back(index, std::move(media));
        }
    }

    void PlayerWindow::stop(smil::InstanceId id)
    {
        blank_screen.end(static_cast<int>(id));
        auto it = instances.find(id);
        if (it == instances.end())
            return;
        for (auto &[index, media] : it->second.shown)
        {
            media->stop();
            std::erase(regions[index].stack, media.get());
            if (media.get() == web_focus)
                setWebFocus(nullptr);
        }
        instances.erase(it);
        std::erase_if(deferred, [id](const Deferred &each) { return each.id == id; });
    }

    void PlayerWindow::forEach(smil::InstanceId id, void (Media::*action)())
    {
        if (auto it = instances.find(id); it != instances.end())
            for (auto &[index, media] : it->second.shown)
                (media.get()->*action)();
    }

    /// A resumed medium comes to the top of its region again
    void PlayerWindow::raise(smil::InstanceId id)
    {
        if (auto it = instances.find(id); it != instances.end())
            for (auto &[index, media] : it->second.shown)
            {
                auto &stack = regions[index].stack;
                if (std::erase(stack, media.get()) > 0)
                    stack.push_back(media.get());
            }
    }

    /// adapi:blankScreen with the standby_mode of the configuration (BlankScreen)
    void PlayerWindow::blankScreen(smil::InstanceId id)
    {
        std::string standby = ask<std::string>(runtime, [](PlayerRuntimeApi &r) { return r.configuration().standbyMode(); });
        blank_screen.begin(static_cast<int>(id), standby,
                           [this, id]
                           {
                               return ask<std::optional<std::chrono::seconds>>(
                                   runtime, [id](PlayerRuntimeApi &r) { return r.remainingTime(id); });
                           });
    }

    /// The web cache if asked, then cachesCleared(): the core restarts the device through the supervisor.
    void PlayerWindow::clearCaches(const tasks::ClearCaches &clear)
    {
        auto done = [this] { runtime.post([](PlayerRuntimeApi &r) { r.cachesCleared(); }); };
        if (!clear.web)
            return done();
        clearWebCache(done);
    }

    /// Mouse events for the web page under the pointer (window coordinates x, y); a press there gives it the
    /// keyboard, a press elsewhere takes it away
    void PlayerWindow::forwardMouse(const SDL_Event &event, float x, float y)
    {
        if (menu.isOpen() || dialog)
            return;
        float pixels = SDL_GetWindowPixelDensity(window);
        x *= pixels;
        y *= pixels;
        Media *target = nullptr;
        SDL_FRect area{};
        for (auto region = regions.rbegin(); region != regions.rend(); ++region)
        {
            area = regionArea(*region->definition);
            if (x < area.x || y < area.y || x >= area.x + area.w || y >= area.y + area.h)
                continue;
            if (!region->stack.empty() && region->stack.back()->interactive())
                target = region->stack.back();
            break;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            setWebFocus(target);
        if (target != nullptr)
            target->input(event, x - area.x, y - area.y);
    }

    void PlayerWindow::setWebFocus(Media *media)
    {
        if (media == web_focus)
            return;
        web_focus = media;
        if (dialog) // the dialog keeps the text input as long as it is open
            return;
        if (web_focus != nullptr)
            SDL_StartTextInput(window);
        else
            SDL_StopTextInput(window);
    }

    void PlayerWindow::clear()
    {
        while (!instances.empty())
            stop(instances.begin()->first);
    }

    /// What the media reported, after the event that caused it
    void PlayerWindow::report()
    {
        std::vector<Deferred> taken;
        taken.swap(deferred);
        for (const Deferred &each : taken)
        {
            smil::InstanceId id = each.id;
            if (each.outcome == Outcome::Finished)
                runtime.post([id](PlayerRuntimeApi &r) { r.mediaFinished(id); });
            else
            {
                std::cerr << "media failed: " << each.message << "\n";
                runtime.post([id](PlayerRuntimeApi &r) { r.mediaFailed(id); });
            }
        }
    }

    // ------------------------------------------------------------------ menu and dialogs

    void PlayerWindow::openMenu()
    {
        if (is_locked || dialog)
            return;
        menu.open(menu_position, windowPoints());
        updateCursor();
        dirty = true;
    }

    void PlayerWindow::openSettingsDialog(bool required)
    {
        if (is_locked && !required)
            return;
        menu.close();
        settings_required = required;
        dialog = std::make_unique<SettingsDialog>(runtime, identity_locked); // the core stores and loads the index again
        SDL_StartTextInput(window);
        next_tick = Clock::now() + std::chrono::seconds(1);
        updateCursor();
        dirty = true;
    }

    void PlayerWindow::openDebugDialog()
    {
        if (is_locked)
            return;
        menu.close();
        settings_required = false;
        dialog    = std::make_unique<DebugDialog>(runtime, [this] { return playingMedia(); });
        next_tick = Clock::now() + std::chrono::seconds(1);
        updateCursor();
        dirty = true;
    }

    /// Closes the dialog once it has a result
    void PlayerWindow::closeDialog()
    {
        if (!dialog || dialog->result() == Dialog::Result::Open)
            return;
        Dialog::Result result = dialog->result();
        dialog.reset();
        if (web_focus != nullptr) // the web page gets the keyboard back
            SDL_StartTextInput(window);
        else
            SDL_StopTextInput(window);
        updateCursor();
        dirty = true;
        if (settings_required && result != Dialog::Result::Accepted)
        {
            exit_code = 1; // without an index nothing can be played (garlic-lib)
            running   = false;
        }
        settings_required = false;
    }

    void PlayerWindow::quit()
    {
        if (quit_hook)
            quit_hook();
        running = false;
    }

    void PlayerWindow::setLocked(bool locked)
    {
        is_locked = locked;
        if (!locked)
            return;
        hold_until.reset();
        menu.close();
        if (dialog && !settings_required)
        {
            dialog.reset();
            if (web_focus == nullptr)
                SDL_StopTextInput(window);
        }
        updateCursor();
        dirty = true;
    }

    void PlayerWindow::applyConfiguration(const std::string &path)
    {
        runtime.post([path](PlayerRuntimeApi &r) { r.applyConfigurationFile(path); });
    }

    void PlayerWindow::playIndex(const std::string &uri)
    {
        runtime.post(
            [uri](PlayerRuntimeApi &r)
            {
                auto valid = configuration::Configuration::validateIndexUri(uri);
                if (!valid)
                {
                    std::cerr << "index not taken: " << valid.error() << "\n";
                    return;
                }
                PlayerSettings next = r.playerSettings();
                next.index_uri      = *valid;
                if (auto changed = r.changePlayerSettings(next); !changed)
                    std::cerr << "index not taken: " << changed.error() << "\n";
            });
    }

    std::vector<std::string> PlayerWindow::playingMedia() const
    {
        std::vector<std::string> lines;
        for (const auto &[id, instance] : instances)
            for (const auto &[index, media] : instance.shown)
            {
                const smil::Region &definition = *regions[index].definition;
                lines.push_back((definition.region_name.empty() ? definition.id : definition.region_name) + ": " +
                                media->label());
            }
        return lines;
    }

    // ------------------------------------------------------------------ window

    void PlayerWindow::setMode(WindowMode next, int screen)
    {
        mode = next;
        SDL_DisplayID display = SDL_GetDisplayForWindow(window);
        int           count   = 0;
        SDL_DisplayID *displays = SDL_GetDisplays(&count);
        if (screen >= 0)
            display = displays != nullptr && screen < count ? displays[screen] : SDL_GetPrimaryDisplay();
        SDL_Rect bounds{};
        SDL_GetDisplayBounds(display, &bounds);

        if (next == WindowMode::Bigscreen) // garlic-lib: one frameless window over all screens
        {
            SDL_Rect all = bounds;
            for (int i = 0; displays != nullptr && i < count; ++i)
            {
                SDL_Rect each{};
                SDL_GetDisplayBounds(displays[i], &each);
                SDL_GetRectUnion(&all, &each, &all);
            }
            SDL_SetWindowFullscreen(window, false);
            SDL_SetWindowBordered(window, false);
            SDL_SetWindowAlwaysOnTop(window, true);
            SDL_SetWindowPosition(window, all.x, all.y);
            SDL_SetWindowSize(window, all.w, all.h);
        }
        else if (next == WindowMode::Fullscreen)
        {
            SDL_SetWindowAlwaysOnTop(window, false);
            SDL_SetWindowBordered(window, true);
            SDL_SetWindowPosition(window, bounds.x, bounds.y);
            SDL_SetWindowFullscreenMode(window, nullptr); // the desktop mode of the screen
            SDL_SetWindowFullscreen(window, true);
        }
        else
        {
            SDL_SetWindowFullscreen(window, false);
            SDL_SetWindowAlwaysOnTop(window, false);
            SDL_SetWindowBordered(window, true);
            SDL_SetWindowSize(window, windowed_width, windowed_height);
            if (screen >= 0)
                SDL_SetWindowPosition(window, bounds.x, bounds.y);
        }
        SDL_free(displays);
        updateCursor();
        dirty = true;
    }

    /// Hidden on a full screen, shown in a window and while the menu or a dialog is open
    void PlayerWindow::updateCursor()
    {
        if (mode == WindowMode::Windowed || menu.isOpen() || dialog)
            SDL_ShowCursor();
        else
            SDL_HideCursor();
    }

    SDL_FRect PlayerWindow::regionArea(const smil::Region &definition) const
    {
        int width  = 0;
        int height = 0;
        SDL_GetCurrentRenderOutputSize(renderer, &width, &height);
        auto w = static_cast<float>(width);
        auto h = static_cast<float>(height);
        return {std::round(w * static_cast<float>(definition.left)), std::round(h * static_cast<float>(definition.top)),
                std::round(w * static_cast<float>(definition.width)), std::round(h * static_cast<float>(definition.height))};
    }

    /// Pixels per point of the menu and the dialogs: at least the density of the screen, and larger on large screens,
    /// so they keep their size relative to a screen of 900 pixels height
    float PlayerWindow::uiScale() const
    {
        int width  = 0;
        int height = 0;
        SDL_GetCurrentRenderOutputSize(renderer, &width, &height);
        return std::max({1.0F, SDL_GetWindowDisplayScale(window), static_cast<float>(height) / 900.0F});
    }

    ui::Rect PlayerWindow::windowPoints() const
    {
        int width  = 0;
        int height = 0;
        SDL_GetCurrentRenderOutputSize(renderer, &width, &height);
        float scale = uiScale();
        return {0, 0, static_cast<float>(width) / scale, static_cast<float>(height) / scale};
    }

    /// Color, then the image at its own size from the top left corner, repeated like the CSS of the Qt players
    void PlayerWindow::drawBackground(SDL_Texture *texture, const std::string &color, const std::string &repeat,
                                      const SDL_FRect &area)
    {
        if (auto fill = parseColor(color); fill && fill->alpha > 0)
        {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, fill->red, fill->green, fill->blue, fill->alpha);
            SDL_RenderFillRect(renderer, &area);
        }
        if (texture == nullptr)
            return;
        float width  = 0;
        float height = 0;
        SDL_GetTextureSize(texture, &width, &height);
        if (width <= 0 || height <= 0)
            return;
        std::string how;
        for (char c : repeat)
            how += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        bool  across = how != "repeaty" && how != "norepeat";
        bool  down   = how != "repeatx" && how != "norepeat";
        float right  = across ? area.x + area.w : area.x + 1;
        float bottom = down ? area.y + area.h : area.y + 1;
        for (float y = area.y; y < bottom; y += height)
            for (float x = area.x; x < right; x += width)
            {
                SDL_FRect tile{x, y, width, height};
                SDL_RenderTexture(renderer, texture, nullptr, &tile);
            }
    }

    void PlayerWindow::render()
    {
        dirty = false;
        SDL_SetRenderClipRect(renderer, nullptr);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        int width  = 0;
        int height = 0;
        SDL_GetCurrentRenderOutputSize(renderer, &width, &height);
        SDL_FRect whole{0, 0, static_cast<float>(width), static_cast<float>(height)};
        if (layout.document)
        {
            const smil::RootLayout &root = layout.document->head.root_layout;
            drawBackground(root_background, root.background_color, root.background_repeat, whole);
        }
        for (Region &region : regions)
        {
            SDL_FRect area = regionArea(*region.definition);
            SDL_Rect  clip{static_cast<int>(area.x), static_cast<int>(area.y), static_cast<int>(area.w),
                           static_cast<int>(area.h)};
            SDL_SetRenderClipRect(renderer, &clip);
            drawBackground(region.background, region.definition->background_color,
                           region.definition->background_repeat, area);
            if (!region.stack.empty())
                region.stack.back()->draw(*renderer, area);
        }
        SDL_SetRenderClipRect(renderer, nullptr);
        drawLoading();

        if (screenshot_path)
        {
            std::string path = *screenshot_path;
            screenshot_path.reset();
            saveScreenshot(path);
        }

        if (menu.isOpen() || dialog)
        {
            canvas->setScale(uiScale());
            if (dialog)
            {
                canvas->fillRect(windowPoints(), {0, 0, 0, 120});
                dialog->layout(windowPoints());
                dialog->draw(*canvas);
            }
            menu.draw(*canvas);
        }
        SDL_RenderPresent(renderer);
    }

    /**
     * While downloads run and no medium was shown since the layout: "Loading 2 / 7" and a bar in the middle of the
     * screen, so a screen is not black while the first files come (a 4K video takes minutes). Gone with the first
     * medium and not back in a gap between media later, which would flash; again with the next layout.
     */
    void PlayerWindow::drawLoading()
    {
        for (const Region &region : regions)
            if (!region.stack.empty())
                media_shown = true;
        if (downloads.total == 0 || media_shown)
            return;
        canvas->setScale(uiScale());
        ui::Rect    window = windowPoints();
        std::string label  = "Loading " + std::to_string(std::min(downloads.done + 1, downloads.total)) + " / " +
                            std::to_string(downloads.total);
        float width = std::min(400.0F, window.width - 40.0F);
        float x     = window.x + (window.width - width) / 2;
        float y     = window.y + window.height / 2;
        canvas->text({window.x + (window.width - canvas->textWidth(label)) / 2, y - canvas->textHeight() - 12}, label,
                     ui::look::text);
        ui::Rect bar{x, y, width, 10};
        canvas->fillRect(bar, ui::look::field);
        // the bytes of the first download, so the bar moves at once; without a size the files
        float fraction = downloads.bytes_total > 0
                             ? static_cast<float>(downloads.bytes_done) / static_cast<float>(downloads.bytes_total)
                             : static_cast<float>(downloads.done) / static_cast<float>(downloads.total);
        canvas->fillRect({x, y, width * std::min(fraction, 1.0F), 10}, ui::look::highlight_top);
    }

    /// garlic-lib: the whole screen as JPEG with quality 60; here the window, which is the screen in full screen mode
    void PlayerWindow::saveScreenshot(const std::string &path)
    {
        if (SDL_Surface *surface = SDL_RenderReadPixels(renderer, nullptr))
        {
            if (!IMG_SaveJPG(surface, path.c_str(), 60))
                std::cerr << "screenshot not saved: " << SDL_GetError() << "\n";
            SDL_DestroySurface(surface);
        }
        runtime.post([path](PlayerRuntimeApi &r) { r.screenshotTaken(path); }); // also if it failed: the core answers
    }
}
