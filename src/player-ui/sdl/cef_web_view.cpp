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
 * WebView with CEF off-screen rendering (Linux, Windows, macOS): the page is painted into a buffer (OnPaint), which goes into a
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
#ifdef __APPLE__
#include "include/wrapper/cef_library_loader.h"
#endif

#include <SDL3/SDL.h>
#ifdef __linux__
#include <SDL3/SDL_egl.h>
#endif
#include <SDL3/SDL_opengl.h>

#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __GLIBC__
#include <malloc.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
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
        std::string                ozone = "headless"; // the platform of CEF on Linux, see initialize()
        std::string                video_driver;       // of SDL: x11, wayland, kmsdrm …
#ifdef __APPLE__
        bool framework_loaded = false; // runWebHelperProcess(): the framework is loaded at run time, not linked
#endif

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
                    if (!process_type.empty()) // the helper processes get the switches of the browser process
                        return;
#ifdef __linux__ // Windows and macOS: the defaults of Chromium (ANGLE on D3D11, Metal)
                    command_line->AppendSwitchWithValue("ozone-platform", ozone);
                    // ANGLE on OpenGL, the default of Chromium; with Vulkan the GPU process of a Raspberry Pi 4 crashed
                    // under Wayland and KMSDRM, and after the fallbacks Chromium ended the player ("GPU process isn't
                    // usable"). X11: Vulkan, the OpenGL backend of ANGLE would reach for the X server (GLX) and the
                    // GPU process failed. KMSDRM: gl-egl, OpenGL through EGL without an X server; with
                    // gl the GPU process failed for want of an X display, SwiftShader crashed and CEF ran without GPU
                    // (--use-gl=disabled). GARLIC_WEB_ANGLE overrides it (gl, gl-egl, vulkan, swiftshader …)
                    const char *angle = std::getenv("GARLIC_WEB_ANGLE");
                    command_line->AppendSwitchWithValue("use-angle", angle != nullptr && *angle != '\0' ? angle
                                                                     : video_driver == "x11"            ? "vulkan"
                                                                     : video_driver == "kmsdrm"         ? "gl-egl"
                                                                                                        : "gl");
#endif
                    // QTWEBENGINE_CHROMIUM_FLAGS of the Qt players: widgets read local files and remote data
                    command_line->AppendSwitch("allow-file-access-from-files");
                    command_line->AppendSwitch("disable-web-security");
                    // web pages always light, like the Qt players: Chromium follows the dark scheme of the desktop
                    command_line->AppendSwitchWithValue("blink-settings", "preferredColorScheme=1");
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

        /// The folder of the program, where CMake puts libcef, the resources and the locales (not on macOS)
        std::string programDir()
        {
            const char *base = SDL_GetBasePath();
            return base != nullptr ? base : "";
        }

        /// Windows: CEF takes the instance of the program, the command line it reads itself
        CefMainArgs mainArgs([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
        {
#ifdef _WIN32
            return CefMainArgs(GetModuleHandleW(nullptr));
#else
            return CefMainArgs(argc, argv);
#endif
        }

        bool sharedTexturesUsable(SDL_Renderer &renderer);

        /// Set by clearWebCache(): the next start removes the profile of CEF (beside it, not in it)
        std::string clearMark()
        {
            return standardPaths().data_dir + "/cef.clear";
        }

        /**
         * CEF once, at the first web page. Its platform (ozone) follows the GPU path: the shared textures need the one
         * of SDL (Wayland or X11), whose GPU buffers (GBM) the GPU process allocates them from; headless has none, the
         * frames would never come. Without them headless, so CEF needs no display (KMSDRM on a Raspberry Pi).
         */
        bool initialize(SDL_Renderer &renderer)
        {
            if (initialized || failed)
                return initialized;
#ifdef __APPLE__
            if (!framework_loaded)
            {
                failed = true;
                return false;
            }
#endif
            std::string driver = SDL_GetCurrentVideoDriver() != nullptr ? SDL_GetCurrentVideoDriver() : "";
            video_driver       = driver;
            if (sharedTexturesUsable(renderer))
                ozone = driver == "wayland" || driver == "x11" ? driver : "headless";
            cache_root = standardPaths().data_dir + "/cef";
            std::error_code error;
            if (std::filesystem::exists(clearMark(), error)) // clear_webcache: the whole profile, before CEF uses it
            {
                std::filesystem::remove_all(cache_root, error);
                std::filesystem::remove(clearMark(), error);
            }
            std::filesystem::create_directories(cache_root, error);

            CefMainArgs        args = mainArgs(0, nullptr);
            CefSettings        settings;
            settings.no_sandbox                   = true; // chrome-sandbox would have to be setuid root
            settings.windowless_rendering_enabled = true;
            settings.multi_threaded_message_loop  = false;
            settings.external_message_pump        = true;
            settings.persist_session_cookies      = true;
            settings.log_severity                 = LOGSEVERITY_WARNING;
            CefString(&settings.root_cache_path)  = cache_root;
            CefString(&settings.cache_path)       = cache_root;
            CefString(&settings.log_file)         = standardPaths().log_dir + "/cef.log";
#ifndef __APPLE__ // macOS: CEF finds them in the bundle (the framework and the helper apps in Contents/Frameworks)
            std::string dir = programDir();
            CefString(&settings.resources_dir_path) = dir;
            CefString(&settings.locales_dir_path)   = dir + "locales";
#endif
#ifdef __linux__ // Windows: the program itself is the default
            CefString(&settings.browser_subprocess_path) = dir + "garlic-player";
#endif
            if (!CefInitialize(args, settings, app().get(), nullptr))
            {
                std::cerr << "web pages: CEF cannot start\n";
                failed = true;
                return false;
            }
            initialized = true;
            return true;
        }

        // ------------------------------------------------------------ shared textures (OnAcceleratedPaint)

#ifdef __linux__

        /**
         * The GPU path: CEF hands every frame as dmabuf, which is imported as EGL image and copied on the GPU
         * (glBlitFramebuffer) into the texture of the page, in the context of the SDL renderer. No read back to the
         * CPU, no upload. Needs EGL (Wayland, KMSDRM; on X11 the hint SDL_VIDEO_FORCE_EGL of prepareWebViews()),
         * EGL_EXT_image_dma_buf_import, GL_OES_EGL_image and glBlitFramebuffer (OpenGL 3, OpenGL ES 3). Found once at
         * the first web page; without it the pages come through OnPaint, copied by the CPU.
         */
        struct SharedTextures
        {
                bool           decided        = false;
                bool           usable         = false;
                bool           with_modifiers = false; // EGL_EXT_image_dma_buf_import_modifiers
                SDL_EGLDisplay display        = nullptr;

                PFNEGLCREATEIMAGEKHRPROC  createImage  = nullptr;
                PFNEGLDESTROYIMAGEKHRPROC destroyImage = nullptr;
                void (*imageTargetTexture2D)(GLenum, void *)                                       = nullptr;
                void (*genTextures)(GLsizei, GLuint *)                                             = nullptr;
                void (*deleteTextures)(GLsizei, const GLuint *)                                    = nullptr;
                void (*bindTexture)(GLenum, GLuint)                                                = nullptr;
                void (*genFramebuffers)(GLsizei, GLuint *)                                         = nullptr;
                void (*deleteFramebuffers)(GLsizei, const GLuint *)                                = nullptr;
                void (*bindFramebuffer)(GLenum, GLuint)                                            = nullptr;
                void (*framebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint)                = nullptr;
                GLenum (*checkFramebufferStatus)(GLenum)                                           = nullptr;
                void (*blitFramebuffer)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield,
                                        GLenum)                                                    = nullptr;
                void (*getIntegerv)(GLenum, GLint *)                                               = nullptr;
                GLboolean (*isEnabled)(GLenum)                                                     = nullptr;
                void (*enable)(GLenum)                                                             = nullptr;
                void (*disable)(GLenum)                                                            = nullptr;
                void (*flush)()                                                                    = nullptr;
        };

        SharedTextures &shared()
        {
            static SharedTextures functions;
            return functions;
        }

        bool hasWord(const char *list, std::string_view word)
        {
            std::string_view all = list != nullptr ? list : "";
            for (std::size_t at = all.find(word); at != std::string_view::npos; at = all.find(word, at + 1))
            {
                bool start = at == 0 || all[at - 1] == ' ';
                bool end   = at + word.size() == all.size() || all[at + word.size()] == ' ';
                if (start && end)
                    return true;
            }
            return false;
        }

        /// Whether the GPU path can be used, decided at the first web page (logged once)
        bool sharedTexturesUsable(SDL_Renderer &renderer)
        {
            SharedTextures &t = shared();
            if (t.decided)
                return t.usable;
            t.decided = true;
            std::string driver = SDL_GetCurrentVideoDriver() != nullptr ? SDL_GetCurrentVideoDriver() : "";
            auto no = [](const std::string &why)
            {
                std::cerr << "web pages: copied by the CPU (OnPaint), " << why << "\n";
                return false;
            };
            if (const char *off = std::getenv("GARLIC_WEB_SHARED_TEXTURE"); off != nullptr && std::string(off) == "0")
                return no("GARLIC_WEB_SHARED_TEXTURE=0");
            std::string name = SDL_GetRendererName(&renderer);
            if (name != "opengl" && name != "opengles2")
                return no("the renderer is " + name);
            if (driver != "wayland" && driver != "x11")
                return no("CEF has no GPU buffers under the video driver " + driver);
            SDL_FlushRenderer(&renderer); // its context is the current one
            t.display = SDL_EGL_GetCurrentDisplay();
            if (t.display == nullptr)
                return no("SDL renders without EGL");
            auto queryString = reinterpret_cast<const char *(*)(SDL_EGLDisplay, SDL_EGLint)>(
                SDL_EGL_GetProcAddress("eglQueryString"));
            const char *extensions = queryString != nullptr ? queryString(t.display, EGL_EXTENSIONS) : nullptr;
            if (!hasWord(extensions, "EGL_EXT_image_dma_buf_import") || !hasWord(extensions, "EGL_KHR_image_base"))
                return no("no EGL_EXT_image_dma_buf_import");
            t.with_modifiers = hasWord(extensions, "EGL_EXT_image_dma_buf_import_modifiers");
            if (!SDL_GL_ExtensionSupported("GL_OES_EGL_image"))
                return no("no GL_OES_EGL_image");

            auto egl = [](auto &function, const char *symbol)
            {
                function = reinterpret_cast<std::remove_reference_t<decltype(function)>>(SDL_EGL_GetProcAddress(symbol));
            };
            auto get = [](auto &function, const char *symbol)
            {
                function = reinterpret_cast<std::remove_reference_t<decltype(function)>>(SDL_GL_GetProcAddress(symbol));
            };
            egl(t.createImage, "eglCreateImageKHR");
            egl(t.destroyImage, "eglDestroyImageKHR");
            get(t.imageTargetTexture2D, "glEGLImageTargetTexture2DOES");
            get(t.genTextures, "glGenTextures");
            get(t.deleteTextures, "glDeleteTextures");
            get(t.bindTexture, "glBindTexture");
            get(t.genFramebuffers, "glGenFramebuffers");
            get(t.deleteFramebuffers, "glDeleteFramebuffers");
            get(t.bindFramebuffer, "glBindFramebuffer");
            get(t.framebufferTexture2D, "glFramebufferTexture2D");
            get(t.checkFramebufferStatus, "glCheckFramebufferStatus");
            get(t.blitFramebuffer, "glBlitFramebuffer");
            get(t.getIntegerv, "glGetIntegerv");
            get(t.isEnabled, "glIsEnabled");
            get(t.enable, "glEnable");
            get(t.disable, "glDisable");
            get(t.flush, "glFlush");
            if (!t.createImage || !t.destroyImage || !t.imageTargetTexture2D || !t.genTextures || !t.deleteTextures ||
                !t.bindTexture || !t.genFramebuffers || !t.deleteFramebuffers || !t.bindFramebuffer ||
                !t.framebufferTexture2D || !t.checkFramebufferStatus || !t.blitFramebuffer || !t.getIntegerv ||
                !t.isEnabled || !t.enable || !t.disable || !t.flush)
                return no("OpenGL without glBlitFramebuffer or EGL images");
            std::cerr << "web pages: shared textures, copied on the GPU (OnAcceleratedPaint)\n";
            t.usable = true;
            return true;
        }

        /**
         * Copies a frame of OnAcceleratedPaint into the texture (OpenGL name and target) on the GPU: the dmabuf
         * planes as EGL image, attached to a framebuffer, blitted and submitted before the callback returns (then the
         * buffer goes back to CEF), the image destroyed. The renderer is flushed first; SDL sets its state anew
         * afterwards, the framebuffers bound before are bound again. false: the import failed.
         */
        bool copySharedFrame(SDL_Renderer &renderer, const CefAcceleratedPaintInfo &info, int width, int height,
                             GLuint texture, GLenum target)
        {
            SharedTextures &t = shared();
            if (info.plane_count < 1 || info.plane_count > kAcceleratedPaintMaxPlanes)
                return false;
            // CEF RGBA (bytes R, G, B, A) is the DRM format ABGR8888, BGRA is ARGB8888
            constexpr auto fourcc = [](char a, char b, char c, char d)
            {
                return static_cast<SDL_EGLint>(static_cast<std::uint32_t>(a) | (static_cast<std::uint32_t>(b) << 8U) |
                                               (static_cast<std::uint32_t>(c) << 16U) |
                                               (static_cast<std::uint32_t>(d) << 24U));
            };
            SDL_EGLint format =
                info.format == CEF_COLOR_TYPE_RGBA_8888 ? fourcc('A', 'B', '2', '4') : fourcc('A', 'R', '2', '4');

            constexpr std::uint64_t modifier_invalid = 0x00FFFFFFFFFFFFFFull; // DRM_FORMAT_MOD_INVALID
            const SDL_EGLint plane_fd[]     = {EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE1_FD_EXT,
                                               EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE3_FD_EXT};
            const SDL_EGLint plane_offset[] = {EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT,
                                               EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT};
            const SDL_EGLint plane_pitch[]  = {EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE1_PITCH_EXT,
                                               EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT};
            const SDL_EGLint plane_mod_lo[] = {EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
                                               EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT};
            const SDL_EGLint plane_mod_hi[] = {EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
                                               EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT};
            std::vector<SDL_EGLint> attributes{EGL_WIDTH, width, EGL_HEIGHT, height, EGL_LINUX_DRM_FOURCC_EXT, format};
            for (int plane = 0; plane < info.plane_count; ++plane)
            {
                const cef_accelerated_paint_native_pixmap_plane_t &p = info.planes[plane];
                attributes.insert(attributes.end(), {plane_fd[plane], p.fd, plane_offset[plane],
                                                     static_cast<SDL_EGLint>(p.offset), plane_pitch[plane],
                                                     static_cast<SDL_EGLint>(p.stride)});
                if (t.with_modifiers && info.modifier != modifier_invalid)
                    attributes.insert(attributes.end(),
                                      {plane_mod_lo[plane], static_cast<SDL_EGLint>(info.modifier & 0xFFFFFFFFU),
                                       plane_mod_hi[plane], static_cast<SDL_EGLint>(info.modifier >> 32U)});
            }
            attributes.push_back(EGL_NONE);

            SDL_FlushRenderer(&renderer); // SDL's commands first; its state is set anew afterwards
            EGLImageKHR image = t.createImage(t.display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr,
                                              attributes.data());
            if (image == EGL_NO_IMAGE_KHR)
                return false;

            GLint read_before = 0;
            GLint draw_before = 0;
            t.getIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_before);
            t.getIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_before);
            GLuint source = 0;
            t.genTextures(1, &source);
            t.bindTexture(GL_TEXTURE_2D, source);
            t.imageTargetTexture2D(GL_TEXTURE_2D, image);
            t.bindTexture(GL_TEXTURE_2D, 0);

            GLuint framebuffers[2]{};
            t.genFramebuffers(2, framebuffers);
            t.bindFramebuffer(GL_READ_FRAMEBUFFER, framebuffers[0]);
            t.framebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, source, 0);
            t.bindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffers[1]);
            t.framebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, texture, 0);
            bool complete = t.checkFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE &&
                            t.checkFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
            if (complete)
            {
                GLboolean scissor = t.isEnabled(GL_SCISSOR_TEST); // the clip of SDL would cut the copy
                t.disable(GL_SCISSOR_TEST);
                t.blitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                if (scissor)
                    t.enable(GL_SCISSOR_TEST);
                // submitted, not waited for: CEF takes the buffer back after the callback, but reuses it only
                // frames later (a pool), and the dmabuf orders the access of both devices. glFinish here cost about
                // 8 % CPU of the player on NVIDIA (the driver waits busily).
                t.flush();
            }
            t.bindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read_before));
            t.bindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw_before));
            t.deleteFramebuffers(2, framebuffers);
            t.deleteTextures(1, &source);
            t.destroyImage(t.display, image);
            return complete;
        }

#else
        // Windows, macOS: the pages through OnPaint (CEF hands D3D11 textures or IOSurfaces there, not imported)
        struct SharedTextures
        {
                bool usable = false;
        };

        SharedTextures &shared()
        {
            static SharedTextures functions;
            return functions;
        }

        bool sharedTexturesUsable(SDL_Renderer & /*renderer*/)
        {
            return false;
        }
#endif

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
                void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList &dirty,
                                        const CefAcceleratedPaintInfo &info) override;

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
                    closeBrowser();
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
                    shared_texture              = sharedTexturesUsable(*context.renderer);
                    info.shared_texture_enabled = shared_texture;
                    CefBrowserSettings settings;
                    settings.windowless_frame_rate = 60;
                    // opaque white like Chromium in a window (garlic-lib, Qt players). Off-screen it is transparent by
                    // default, and text drawn on transparency comes out pale and washed out
                    settings.background_color = CefColorSetARGB(255, 255, 255, 255);
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
                    // no frame on the GPU path, e.g. X11 without DRI3 (no GBM): the page through OnPaint
                    if (shared_texture && browser && !got_shared_frame &&
                        std::chrono::steady_clock::now() - created_at > std::chrono::seconds(10))
                    {
                        if (shared().usable)
                            std::cerr << "web pages: no frames as shared textures, copied by the CPU from now on\n";
                        shared().usable        = false;
                        restart_without_shared = true;
                    }
                    if (restart_without_shared) // the GPU path failed: the page again, through OnPaint
                    {
                        restart_without_shared = false;
                        closeBrowser();
                        client = new Client(this);
                        load(pending_url);
                    }
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
                        // CEF paints BGRA, which is ARGB8888 on a little endian machine
                        if (prepareTexture(SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, frame_width,
                                           frame_height))
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

                /// A frame on the GPU: copied into the texture of the page now, the buffer lives only in the callback
                void acceleratedPaint([[maybe_unused]] const CefAcceleratedPaintInfo &info)
                {
#ifdef __linux__
                    int w = info.extra.coded_size.width > 0 ? info.extra.coded_size.width : width;
                    int h = info.extra.coded_size.height > 0 ? info.extra.coded_size.height : height;
                    if (!prepareTexture(SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, w, h))
                        return;
                    SDL_PropertiesID properties = SDL_GetTextureProperties(texture);
                    auto id =
                        static_cast<GLuint>(SDL_GetNumberProperty(properties, SDL_PROP_TEXTURE_OPENGL_TEXTURE_NUMBER, 0));
                    auto target = static_cast<GLenum>(
                        SDL_GetNumberProperty(properties, SDL_PROP_TEXTURE_OPENGL_TEXTURE_TARGET_NUMBER, GL_TEXTURE_2D));
                    if (id == 0) // OpenGL ES
                    {
                        id     = static_cast<GLuint>(
                            SDL_GetNumberProperty(properties, SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_NUMBER, 0));
                        target = static_cast<GLenum>(
                            SDL_GetNumberProperty(properties, SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_TARGET_NUMBER, GL_TEXTURE_2D));
                    }
                    if (id == 0 || !copySharedFrame(*context.renderer, info, w, h, id, target))
                    {
                        // logged once for all pages; this page starts again without shared textures
                        if (shared().usable)
                            std::cerr << "web pages: a shared texture cannot be imported, copied by the CPU from now on\n";
                        shared().usable        = false;
                        restart_without_shared = true;
                        if (context.redraw)
                            context.redraw();
                        return;
                    }
                    got_shared_frame = true;
                    if (context.redraw)
                        context.redraw();
#endif
                }

                void created(CefRefPtr<CefBrowser> created_browser)
                {
                    created_at = std::chrono::steady_clock::now();
                    browser  = created_browser;
                    creating = false;
                    browser->GetHost()->WasResized();
                    if (url_changed) // load() came again meanwhile
                        browser->GetMainFrame()->LoadURL(pending_url);
                    url_changed = false;
                }

            private:
                /// The texture in the size and kind of the frame (streaming for OnPaint, target for the GPU copy)
                bool prepareTexture(SDL_PixelFormat format, SDL_TextureAccess access, int w, int h)
                {
                    if (texture == nullptr || texture_width != w || texture_height != h || texture_format != format)
                    {
                        if (texture != nullptr)
                            SDL_DestroyTexture(texture);
                        texture        = SDL_CreateTexture(context.renderer, format, access, w, h);
                        texture_width  = w;
                        texture_height = h;
                        texture_format = format;
                        if (texture != nullptr)
                            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
                    }
                    return texture != nullptr;
                }

                void closeBrowser()
                {
                    client->detach();
                    if (browser)
                        browser->GetHost()->CloseBrowser(true);
                    browser  = nullptr;
                    creating = false;
                }

                MediaContext          context;
                CefRefPtr<Client>     client;
                CefRefPtr<CefBrowser> browser;
                bool                  shared_texture         = false;
                bool                  got_shared_frame       = false;
                bool                  restart_without_shared = false;
                std::chrono::steady_clock::time_point created_at;
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
                SDL_PixelFormat            texture_format = SDL_PIXELFORMAT_UNKNOWN;
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

        void Client::OnAcceleratedPaint(CefRefPtr<CefBrowser> /*browser*/, PaintElementType type,
                                        const RectList & /*dirty*/, const CefAcceleratedPaintInfo &info)
        {
            if (page != nullptr && type == PET_VIEW)
                page->acceleratedPaint(info);
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
        if (context.renderer == nullptr || !initialize(*context.renderer))
            return nullptr;
        return std::make_unique<CefPage>(context);
    }

#ifdef __APPLE__
    void prepareCefApplication(); // mac/cef_application.mm
#endif

    /**
     * Linux: on X11 SDL takes GLX; the shared textures of CEF need EGL, which Wayland and KMSDRM use anyway.
     * macOS: the application class of SDL gets what CEF needs of it.
     */
    void prepareWebViews()
    {
#ifdef __linux__
        SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "1");
#elif defined(__APPLE__)
        prepareCefApplication();
#endif
    }

    /// macOS: the subprocesses run in the helper apps, here only the framework is loaded, before any call of CEF
    int runWebHelperProcess([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
    {
#ifdef __APPLE__
        static CefScopedLibraryLoader loader; // until the end of the program
        framework_loaded = loader.LoadInMain();
        if (!framework_loaded)
            std::cerr << "web pages: the CEF framework cannot be loaded\n";
        return -1;
#else
        for (int i = 1; i < argc; ++i)
            if (std::strncmp(argv[i], "--type=", 7) == 0)
            {
                CefMainArgs args = mainArgs(argc, argv);
                // exit here, never return: a process forked by the zygote of Chromium gets a new stack guard
                // (--change-stack-guard-on-fork), so returning through this frame aborted with "stack smashing detected"
                std::exit(CefExecuteProcess(args, app().get(), nullptr));
            }
        return -1; // the player itself
#endif
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

    /**
     * The whole profile goes (HTTP cache, cookies, local storage, IndexedDB, service workers), like app_webview of
     * garlic-lib on Android. Not while CEF runs: only the mark here, the next start removes the profile before
     * CefInitialize(); the device restarts afterwards anyway.
     */
    void clearWebCache(const std::function<void()> &done)
    {
        std::ofstream mark(clearMark());
        if (!mark)
            std::cerr << "CEF: cannot mark the profile for clearing: " << clearMark() << "\n";
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

#if defined(__GLIBC__) && __GLIBC_PREREQ(2, 33)
/**
 * Replaces mallinfo() of glibc for libcef.so (the symbol is exported, see CMakeLists.txt). The Linux builds of CEF are
 * made against glibc 2.31 and so call mallinfo() instead of mallinfo2(): its int fields overflow from 2 GiB of malloc
 * memory of the process (all arenas, also the ones of mpv) and the MallocDumpProvider of Chromium (thread
 * "MemoryInfra") crashes on the negative values with a CHECK. Here the values of mallinfo2() are clamped to INT_MAX,
 * arena + hblkhd together, because Chromium adds both as int. Can go once CEF calls mallinfo2().
 */
extern "C" struct mallinfo mallinfo() noexcept
{
    constexpr std::size_t int_max = INT_MAX;
    const struct mallinfo2 info2  = mallinfo2();
    const std::size_t arena       = std::min(info2.arena, int_max);
    struct mallinfo info{};
    info.arena    = static_cast<int>(arena);
    info.ordblks  = static_cast<int>(std::min(info2.ordblks, int_max));
    info.smblks   = static_cast<int>(std::min(info2.smblks, int_max));
    info.hblks    = static_cast<int>(std::min(info2.hblks, int_max));
    info.hblkhd   = static_cast<int>(std::min(info2.hblkhd, int_max - arena));
    info.usmblks  = static_cast<int>(std::min(info2.usmblks, int_max));
    info.fsmblks  = static_cast<int>(std::min(info2.fsmblks, int_max));
    info.uordblks = static_cast<int>(std::min(info2.uordblks, int_max));
    info.fordblks = static_cast<int>(std::min(info2.fordblks, int_max));
    info.keepcost = static_cast<int>(std::min(info2.keepcost, int_max));
    return info;
}
#endif
