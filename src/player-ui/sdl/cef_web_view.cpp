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
/**
 * WebView with CEF off-screen rendering (Linux): the page is painted into a buffer (OnPaint), which goes into a
 * texture of the SDL renderer, so a web page lies in its region by z-index like an image. CEF runs in the main
 * thread with an external message pump: OnScheduleMessagePumpWork wakes the main loop, which calls pumpWebViews().
 *
 * Like the qwidget player (QtWebEngine): local files may read remote data and other files, media play without a
 * click, permissions (camera, microphone …) are granted, and after loading the page gets no scroll bars.
 */
#include "web_view.hpp"

#include "paths.hpp"

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_command_line.h"

#include <SDL3/SDL.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <vector>

namespace player
{
    namespace
    {
        bool                       initialized = false;
        bool                       failed      = false;
        int                        open_browsers = 0;
        std::atomic_bool           work_due{false};
        std::mutex                 wake_mutex;
        std::function<void()>      wake_loop; // the redraw of the MediaContext: an SDL event from any thread
        std::string                cache_root;

        Uint32 wakeLater(void * /*userdata*/, SDL_TimerID /*timer*/, Uint32 /*interval*/)
        {
            work_due = true;
            std::lock_guard lock(wake_mutex);
            if (wake_loop)
                wake_loop();
            return 0; // once
        }

        /// The browser process: the switches of the qwidget player and the work scheduling of the external pump
        class App final : public CefApp, public CefBrowserProcessHandler
        {
            public:
                CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }

                void OnBeforeCommandLineProcessing(const CefString &process_type,
                                                   CefRefPtr<CefCommandLine> command_line) override
                {
                    // off-screen: no X11 or Wayland connection of its own, in no process, so it runs under any SDL
                    // video driver (Wayland, X11, KMSDRM on a Raspberry Pi without desktop); the GPU process would
                    // otherwise get the platform of the desktop
                    command_line->AppendSwitchWithValue("ozone-platform", "headless");
                    if (!process_type.empty())
                        return;
                    // the GPU through Vulkan: headless, the OpenGL backend of ANGLE would reach for the X server
                    // (GLX) and the GPU process failed; GARLIC_WEB_ANGLE overrides it (gl, gl-egl, swiftshader …)
                    const char *angle = std::getenv("GARLIC_WEB_ANGLE");
                    command_line->AppendSwitchWithValue("use-angle",
                                                        angle != nullptr && *angle != '\0' ? angle : "vulkan");
                    // QTWEBENGINE_CHROMIUM_FLAGS of the Qt players: widgets read local files and remote data
                    command_line->AppendSwitch("allow-file-access-from-files");
                    command_line->AppendSwitch("disable-web-security");
                    // PlaybackRequiresUserGesture false
                    command_line->AppendSwitchWithValue("autoplay-policy", "no-user-gesture-required");
                    // disable-web-security needs its own profile directory
                    command_line->AppendSwitchWithValue("user-data-dir", cache_root);
                    // WebGL in software (SwiftShader) when the GPU process cannot start, instead of no WebGL
                    command_line->AppendSwitch("enable-unsafe-swiftshader");
                }

                /// Any thread: wakes the main loop now or after delay_ms
                void OnScheduleMessagePumpWork(int64_t delay_ms) override
                {
                    if (delay_ms <= 0)
                        wakeLater(nullptr, 0, 0);
                    else
                        SDL_AddTimer(static_cast<Uint32>(std::min<int64_t>(delay_ms, 1000)), &wakeLater, nullptr);
                }

            private:
                IMPLEMENT_REFCOUNTING(App);
        };

        CefRefPtr<App> &app()
        {
            static CefRefPtr<App> instance = new App;
            return instance;
        }

        /// The folder of the program, where CMake puts libcef.so, the resources and the locales
        std::string programDir()
        {
            const char *base = SDL_GetBasePath();
            return base != nullptr ? base : "";
        }

        bool initialize()
        {
            if (initialized || failed)
                return initialized;
            cache_root = standardPaths().data_dir + "/cef";
            std::error_code error;
            std::filesystem::create_directories(cache_root, error);

            std::string        dir = programDir();
            CefMainArgs        args(0, nullptr);
            CefSettings        settings;
            settings.no_sandbox                   = true; // chrome-sandbox would have to be setuid root
            settings.windowless_rendering_enabled = true;
            settings.multi_threaded_message_loop  = false;
            settings.external_message_pump        = true;
            settings.persist_session_cookies      = true;
            settings.log_severity                 = LOGSEVERITY_WARNING;
            CefString(&settings.root_cache_path)  = cache_root;
            CefString(&settings.cache_path)       = cache_root;
            CefString(&settings.resources_dir_path) = dir;
            CefString(&settings.locales_dir_path)   = dir + "locales";
            CefString(&settings.browser_subprocess_path) = dir + "garlic-player-sdl";
            CefString(&settings.log_file)                = standardPaths().log_dir + "/cef.log";
            if (!CefInitialize(args, settings, app().get(), nullptr))
            {
                std::cerr << "web pages: CEF cannot start\n";
                failed = true;
                return false;
            }
            initialized = true;
            return true;
        }

        /// SDL key → Windows virtual key code, which CEF wants on every platform
        int windowsKeyCode(SDL_Keycode key)
        {
            if (key >= SDLK_A && key <= SDLK_Z)
                return 'A' + static_cast<int>(key - SDLK_A);
            if (key >= SDLK_0 && key <= SDLK_9)
                return '0' + static_cast<int>(key - SDLK_0);
            if (key >= SDLK_F1 && key <= SDLK_F12)
                return 0x70 + static_cast<int>(key - SDLK_F1);
            switch (key)
            {
                case SDLK_BACKSPACE: return 0x08;
                case SDLK_TAB: return 0x09;
                case SDLK_RETURN:
                case SDLK_KP_ENTER: return 0x0D;
                case SDLK_ESCAPE: return 0x1B;
                case SDLK_SPACE: return 0x20;
                case SDLK_PAGEUP: return 0x21;
                case SDLK_PAGEDOWN: return 0x22;
                case SDLK_END: return 0x23;
                case SDLK_HOME: return 0x24;
                case SDLK_LEFT: return 0x25;
                case SDLK_UP: return 0x26;
                case SDLK_RIGHT: return 0x27;
                case SDLK_DOWN: return 0x28;
                case SDLK_INSERT: return 0x2D;
                case SDLK_DELETE: return 0x2E;
                case SDLK_LSHIFT:
                case SDLK_RSHIFT: return 0x10;
                case SDLK_LCTRL:
                case SDLK_RCTRL: return 0x11;
                case SDLK_LALT:
                case SDLK_RALT: return 0x12;
                default: return 0;
            }
        }

        uint32_t modifiers(SDL_Keymod mod, SDL_MouseButtonFlags buttons = 0)
        {
            uint32_t flags = 0;
            if ((mod & SDL_KMOD_SHIFT) != 0)
                flags |= EVENTFLAG_SHIFT_DOWN;
            if ((mod & SDL_KMOD_CTRL) != 0)
                flags |= EVENTFLAG_CONTROL_DOWN;
            if ((mod & SDL_KMOD_ALT) != 0)
                flags |= EVENTFLAG_ALT_DOWN;
            if ((buttons & SDL_BUTTON_LMASK) != 0)
                flags |= EVENTFLAG_LEFT_MOUSE_BUTTON;
            if ((buttons & SDL_BUTTON_MMASK) != 0)
                flags |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
            if ((buttons & SDL_BUTTON_RMASK) != 0)
                flags |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
            return flags;
        }

        class CefPage;

        /// The handlers of one browser; refcounted by CEF, so it may outlive its page
        class Client final : public CefClient,
                             public CefRenderHandler,
                             public CefLoadHandler,
                             public CefLifeSpanHandler,
                             public CefPermissionHandler
        {
            public:
                explicit Client(CefPage *owner) : page(owner) {}

                void detach() { page = nullptr; }

                CefRefPtr<CefRenderHandler>     GetRenderHandler() override { return this; }
                CefRefPtr<CefLoadHandler>       GetLoadHandler() override { return this; }
                CefRefPtr<CefLifeSpanHandler>   GetLifeSpanHandler() override { return this; }
                CefRefPtr<CefPermissionHandler> GetPermissionHandler() override { return this; }

                void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect) override;
                void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList &dirty,
                             const void *buffer, int width, int height) override;

                /// garlic-lib, the qwidget player: no scroll bars once the page is there
                void OnLoadEnd(CefRefPtr<CefBrowser> /*browser*/, CefRefPtr<CefFrame> frame, int /*status*/) override
                {
                    if (frame->IsMain())
                        frame->ExecuteJavaScript("document.body.style.overflow = 'hidden';", frame->GetURL(), 0);
                }

                void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
                void OnBeforeClose(CefRefPtr<CefBrowser> /*browser*/) override { --open_browsers; }

                /// No new windows: a signage screen has nowhere to show them
                bool OnBeforePopup(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, int, const CefString &, const CefString &,
                                   WindowOpenDisposition, bool, const CefPopupFeatures &, CefWindowInfo &,
                                   CefRefPtr<CefClient> &, CefBrowserSettings &, CefRefPtr<CefDictionaryValue> &,
                                   bool *) override
                {
                    return true;
                }

                // camera, microphone and the rest granted, like the qwidget player
                bool OnRequestMediaAccessPermission(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, const CefString &,
                                                    uint32_t requested, CefRefPtr<CefMediaAccessCallback> callback) override
                {
                    callback->Continue(requested);
                    return true;
                }

                bool OnShowPermissionPrompt(CefRefPtr<CefBrowser>, uint64_t, const CefString &, uint32_t,
                                            CefRefPtr<CefPermissionPromptCallback> callback) override
                {
                    callback->Continue(CEF_PERMISSION_RESULT_ACCEPT);
                    return true;
                }

            private:
                CefPage *page;
                IMPLEMENT_REFCOUNTING(Client);
        };

        class CefPage final : public WebView
        {
            public:
                explicit CefPage(const MediaContext &c) : context(c), client(new Client(this)) {}

                ~CefPage() override
                {
                    client->detach();
                    if (browser)
                        browser->GetHost()->CloseBrowser(true);
                    if (texture != nullptr)
                        SDL_DestroyTexture(texture);
                }

                void load(const std::string &url) override
                {
                    if (browser)
                    {
                        browser->GetMainFrame()->LoadURL(url);
                        return;
                    }
                    pending_url = url;
                    if (creating)
                    {
                        url_changed = true; // loaded when the browser is there
                        return;
                    }
                    CefWindowInfo info;
                    info.SetAsWindowless(0);
                    CefBrowserSettings settings;
                    settings.windowless_frame_rate = 60;
                    settings.javascript_access_clipboard = STATE_ENABLED; // JavascriptCanPaste
                    settings.javascript_dom_paste        = STATE_ENABLED;
                    creating = CefBrowserHost::CreateBrowser(info, client, url, settings, nullptr, nullptr);
                    if (creating)
                        ++open_browsers;
                }

                void stop() override
                {
                    if (browser)
                        browser->StopLoad();
                }

                void draw(SDL_Renderer &renderer, const SDL_FRect &area) override
                {
                    int w = std::max(1, static_cast<int>(area.w + 0.5F));
                    int h = std::max(1, static_cast<int>(area.h + 0.5F));
                    if (w != width || h != height)
                    {
                        width  = w;
                        height = h;
                        if (browser)
                            browser->GetHost()->WasResized();
                    }
                    if (fresh && !pixels.empty())
                    {
                        if (texture == nullptr || texture_width != frame_width || texture_height != frame_height)
                        {
                            if (texture != nullptr)
                                SDL_DestroyTexture(texture);
                            // CEF paints BGRA, which is ARGB8888 on a little endian machine
                            texture = SDL_CreateTexture(&renderer, SDL_PIXELFORMAT_ARGB8888,
                                                        SDL_TEXTUREACCESS_STREAMING, frame_width, frame_height);
                            texture_width  = frame_width;
                            texture_height = frame_height;
                            if (texture != nullptr)
                                SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
                        }
                        if (texture != nullptr)
                            SDL_UpdateTexture(texture, nullptr, pixels.data(), frame_width * 4);
                        fresh = false;
                    }
                    if (texture == nullptr)
                        return;
                    // the page in its own size from the top left corner; until a resize arrives it may differ
                    SDL_FRect where{area.x, area.y, static_cast<float>(texture_width), static_cast<float>(texture_height)};
                    SDL_RenderTexture(&renderer, texture, nullptr, &where);
                }

                void input(const SDL_Event &event, float x, float y) override
                {
                    if (!browser)
                        return;
                    CefRefPtr<CefBrowserHost> host = browser->GetHost();
                    CefMouseEvent             mouse;
                    mouse.x = static_cast<int>(x);
                    mouse.y = static_cast<int>(y);
                    switch (event.type)
                    {
                        case SDL_EVENT_MOUSE_BUTTON_DOWN:
                        case SDL_EVENT_MOUSE_BUTTON_UP:
                        {
                            mouse.modifiers = modifiers(SDL_GetModState(), SDL_GetMouseState(nullptr, nullptr));
                            cef_mouse_button_type_t button = event.button.button == SDL_BUTTON_RIGHT ? MBT_RIGHT
                                                             : event.button.button == SDL_BUTTON_MIDDLE ? MBT_MIDDLE
                                                                                                        : MBT_LEFT;
                            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                                host->SetFocus(true);
                            host->SendMouseClickEvent(mouse, button, event.type == SDL_EVENT_MOUSE_BUTTON_UP,
                                                      std::max<int>(1, event.button.clicks));
                            break;
                        }
                        case SDL_EVENT_MOUSE_MOTION:
                            mouse.modifiers = modifiers(SDL_GetModState(), event.motion.state);
                            host->SendMouseMoveEvent(mouse, false);
                            break;
                        case SDL_EVENT_MOUSE_WHEEL:
                            mouse.modifiers = modifiers(SDL_GetModState());
                            host->SendMouseWheelEvent(mouse, static_cast<int>(event.wheel.x * 120),
                                                      static_cast<int>(event.wheel.y * 120));
                            break;
                        case SDL_EVENT_KEY_DOWN:
                        case SDL_EVENT_KEY_UP:
                        {
                            CefKeyEvent key;
                            key.type              = event.type == SDL_EVENT_KEY_DOWN ? KEYEVENT_RAWKEYDOWN : KEYEVENT_KEYUP;
                            key.windows_key_code  = windowsKeyCode(event.key.key);
                            key.native_key_code   = static_cast<int>(event.key.scancode);
                            key.modifiers         = modifiers(event.key.mod);
                            host->SendKeyEvent(key);
                            // keys that type no text but act like one: Enter, Backspace, Tab
                            if (event.type == SDL_EVENT_KEY_DOWN &&
                                (key.windows_key_code == 0x0D || key.windows_key_code == 0x08 || key.windows_key_code == 0x09))
                            {
                                key.type      = KEYEVENT_CHAR;
                                key.character = static_cast<char16_t>(key.windows_key_code);
                                key.unmodified_character = key.character;
                                host->SendKeyEvent(key);
                            }
                            break;
                        }
                        case SDL_EVENT_TEXT_INPUT:
                        {
                            CefString      text(event.text.text);
                            std::u16string utf16 = text.ToString16();
                            for (char16_t c : utf16)
                            {
                                CefKeyEvent key;
                                key.type                 = KEYEVENT_CHAR;
                                key.character            = c;
                                key.unmodified_character = c;
                                key.windows_key_code     = c;
                                host->SendKeyEvent(key);
                            }
                            break;
                        }
                        default: break;
                    }
                }

                // from the Client, in the main thread (the message pump runs there)
                void viewRect(CefRect &rect) const { rect = CefRect(0, 0, width, height); }

                void painted(const void *buffer, int w, int h)
                {
                    frame_width  = w;
                    frame_height = h;
                    pixels.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4);
                    std::memcpy(pixels.data(), buffer, pixels.size());
                    fresh = true;
                    if (context.redraw)
                        context.redraw();
                }

                void created(CefRefPtr<CefBrowser> created_browser)
                {
                    browser  = created_browser;
                    creating = false;
                    browser->GetHost()->WasResized();
                    if (url_changed) // load() came again meanwhile
                        browser->GetMainFrame()->LoadURL(pending_url);
                    url_changed = false;
                }

            private:
                MediaContext          context;
                CefRefPtr<Client>     client;
                CefRefPtr<CefBrowser> browser;
                bool                  creating    = false;
                bool                  url_changed = false;
                std::string           pending_url;
                int                   width  = 1;
                int                   height = 1;

                std::vector<unsigned char> pixels;
                int                        frame_width  = 0;
                int                        frame_height = 0;
                bool                       fresh        = false;
                SDL_Texture               *texture        = nullptr;
                int                        texture_width  = 0;
                int                        texture_height = 0;
        };

        void Client::GetViewRect(CefRefPtr<CefBrowser> /*browser*/, CefRect &rect)
        {
            if (page != nullptr)
                page->viewRect(rect);
            else
                rect = CefRect(0, 0, 1, 1);
        }

        void Client::OnPaint(CefRefPtr<CefBrowser> /*browser*/, PaintElementType type, const RectList & /*dirty*/,
                             const void *buffer, int width, int height)
        {
            if (page != nullptr && type == PET_VIEW) // popups (an open <select>) are not drawn
                page->painted(buffer, width, height);
        }

        void Client::OnAfterCreated(CefRefPtr<CefBrowser> browser)
        {
            if (page != nullptr)
                page->created(browser);
            else
                browser->GetHost()->CloseBrowser(true); // the medium went before its browser came
        }
    }

    std::unique_ptr<WebView> createWebView(const MediaContext &context)
    {
        {
            std::lock_guard lock(wake_mutex);
            if (!wake_loop)
                wake_loop = context.redraw;
        }
        if (!initialize())
            return nullptr;
        return std::make_unique<CefPage>(context);
    }

    int runWebHelperProcess(int argc, char *argv[])
    {
        for (int i = 1; i < argc; ++i)
            if (std::strncmp(argv[i], "--type=", 7) == 0)
            {
                CefMainArgs args(argc, argv);
                return CefExecuteProcess(args, app().get(), nullptr);
            }
        return -1; // the player itself
    }

    /// What CEF scheduled, and at least every 33 ms like the external pump of cefclient: CEF does not schedule all
    /// of its work
    void pumpWebViews()
    {
        static auto next = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (!initialized || (!work_due.exchange(false) && now < next))
            return;
        next = now + std::chrono::milliseconds(33);
        CefDoMessageLoopWork();
    }

    bool webViewsRunning()
    {
        return initialized;
    }

    /// Like clearHttpCache of QtWebEngine: the caches of the profile go; the device restarts afterwards anyway
    void clearWebCache(const std::function<void()> &done)
    {
        if (!cache_root.empty())
            for (const char *dir : {"Cache", "Code Cache", "GPUCache", "Default/Cache", "Default/Code Cache"})
            {
                std::error_code error;
                std::filesystem::remove_all(cache_root + "/" + dir, error);
            }
        done();
    }

    /// The browsers close asynchronously: pumped until they are gone (at most 3 s), then CEF ends
    void shutdownWebViews()
    {
        if (!initialized)
            return;
        {
            std::lock_guard lock(wake_mutex); // the window is gone: nobody to wake
            wake_loop = nullptr;
        }
        auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (open_browsers > 0 && std::chrono::steady_clock::now() < until)
        {
            CefDoMessageLoopWork();
            SDL_Delay(10);
        }
        CefShutdown();
        initialized = false;
    }
}
