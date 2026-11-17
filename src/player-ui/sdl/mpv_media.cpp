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
#include "mpv_media.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace player
{
    namespace
    {
        /// The few OpenGL (ES 2) functions for the framebuffer and the fence. On Linux they do not depend on the context.
        struct Gl
        {
                void (*genFramebuffers)(GLsizei, GLuint *)                             = nullptr;
                void (*deleteFramebuffers)(GLsizei, const GLuint *)                    = nullptr;
                void (*bindFramebuffer)(GLenum, GLuint)                                = nullptr;
                void (*framebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint)    = nullptr;
                GLenum (*checkFramebufferStatus)(GLenum)                               = nullptr;
                void (*getIntegerv)(GLenum, GLint *)                                   = nullptr;
                void (*flush)()                                                        = nullptr;
                void (*finish)()                                                       = nullptr;
                void *(*fenceSync)(GLenum, GLbitfield)                                 = nullptr; // GLsync
                void (*waitSync)(void *, GLbitfield, std::uint64_t)                    = nullptr;
                void (*deleteSync)(void *)                                             = nullptr;

                bool load()
                {
                    auto get = [](auto &function, const char *name)
                    { function = reinterpret_cast<std::remove_reference_t<decltype(function)>>(SDL_GL_GetProcAddress(name)); };
                    get(genFramebuffers, "glGenFramebuffers");
                    get(deleteFramebuffers, "glDeleteFramebuffers");
                    get(bindFramebuffer, "glBindFramebuffer");
                    get(framebufferTexture2D, "glFramebufferTexture2D");
                    get(checkFramebufferStatus, "glCheckFramebufferStatus");
                    get(getIntegerv, "glGetIntegerv");
                    get(flush, "glFlush");
                    get(finish, "glFinish");
                    get(fenceSync, "glFenceSync");
                    get(waitSync, "glWaitSync");
                    get(deleteSync, "glDeleteSync");
                    return genFramebuffers && deleteFramebuffers && bindFramebuffer && framebufferTexture2D &&
                           checkFramebufferStatus && getIntegerv && flush && finish;
                }
        };

        Gl &gl()
        {
            static Gl functions;
            static bool loaded = functions.load();
            (void)loaded;
            return functions;
        }

        constexpr GLenum        sync_gpu_commands_complete = 0x9117; // GL_SYNC_GPU_COMMANDS_COMPLETE
        constexpr std::uint64_t timeout_ignored            = 0xFFFFFFFFFFFFFFFFull; // GL_TIMEOUT_IGNORED
        constexpr GLenum        major_version              = 0x821B; // GL_MAJOR_VERSION (OpenGL 3, OpenGL ES 3)

        /**
         * libmpv renders in an OpenGL context of its own that shares its textures with the one of the SDL renderer.
         * In the same context the state SDL leaves (client arrays, programs, bindings) broke the rendering of mpv: on
         * NVIDIA crashes in the driver with hwdec=nvdec and artefacts with nvdec-copy. One context for all media.
         */
        struct MpvGl
        {
                SDL_Window   *window   = nullptr;
                SDL_GLContext sdl      = nullptr; // the one of the renderer
                SDL_GLContext own      = nullptr;
                bool          has_sync = false;   // fences (OpenGL 3.2, OpenGL ES 3), else glFinish
        };

        MpvGl &mpvGl()
        {
            static MpvGl context;
            return context;
        }

        /// Creates the context of mpv on first use, with the attributes the renderer left (OpenGL or OpenGL ES).
        bool ensureMpvContext(SDL_Renderer &renderer)
        {
            MpvGl &c = mpvGl();
            if (c.own != nullptr)
                return true;
            SDL_FlushRenderer(&renderer); // the context of the renderer is the current one
            c.window = SDL_GetRenderWindow(&renderer);
            c.sdl    = SDL_GL_GetCurrentContext();
            if (c.window == nullptr || c.sdl == nullptr || !gl().genFramebuffers)
                return false;
            GLint major = 0;
            gl().getIntegerv(major_version, &major);
            c.has_sync = gl().fenceSync && gl().waitSync && gl().deleteSync &&
                         (major >= 3 || SDL_GL_ExtensionSupported("GL_ARB_sync"));
            SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
            c.own = SDL_GL_CreateContext(c.window); // becomes the current one
            SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
            SDL_GL_MakeCurrent(c.window, c.sdl);
            return c.own != nullptr;
        }

        /// The context of mpv is the current one while it lives; the one of the renderer afterwards.
        class InMpvContext
        {
            public:
                explicit InMpvContext(SDL_Renderer &renderer)
                {
                    SDL_FlushRenderer(&renderer); // SDL's commands first, it sets its state anew afterwards
                    gl().flush();                 // and to the GPU, so the other context sees new textures
                    SDL_GL_MakeCurrent(mpvGl().window, mpvGl().own);
                }
                ~InMpvContext() { SDL_GL_MakeCurrent(mpvGl().window, mpvGl().sdl); }
                InMpvContext(const InMpvContext &)            = delete;
                InMpvContext &operator=(const InMpvContext &) = delete;
        };

        bool isOpenGl(SDL_Renderer &renderer)
        {
            std::string name = SDL_GetRendererName(&renderer);
            return name == "opengl" || name == "opengles2";
        }

        /**
         * The OpenGL functions for mpv, without glFenceSync: libmpv (render API, mpv 0.40 and 0.41; mpv issue 17217,
         * fixed in 0.42) makes a fence for every frame and deletes it only when swapping buffers, which the render
         * API never does. On a Raspberry Pi (Mesa V3D) every fence holds a sync_file, so after about a minute the
         * player ran out of file descriptors (1024) and showed nothing anymore. mpv works without fences (it checks
         * for the function); that SDL reads a frame only when mpv has written it is the fence of draw().
         */
        void *procAddress(void * /*context*/, const char *name)
        {
            if (std::string_view(name) == "glFenceSync")
                return nullptr;
            return reinterpret_cast<void *>(SDL_GL_GetProcAddress(name));
        }
    }

    void shutdownMpv()
    {
        MpvGl &c = mpvGl();
        if (c.own != nullptr)
            SDL_GL_DestroyContext(c.own);
        c = {};
    }

    MpvMedia::MpvMedia(const garlic::core::smil::Media &s, const garlic::core::smil::Region &r, std::string src,
                       Report rep, const MediaContext &c, bool video)
        : Media(s, r, std::move(src), std::move(rep)), context(c), is_video(video)
    {
    }

    MpvMedia::~MpvMedia()
    {
        if (mpv != nullptr)
            mpv_set_wakeup_callback(mpv, nullptr, nullptr);
        if (render != nullptr)
            mpv_render_context_set_update_callback(render, nullptr, nullptr);
        releaseFrame();
        if (render != nullptr)
        {
            InMpvContext in_mpv(*context.renderer);
            mpv_render_context_free(render);
        }
        if (mpv != nullptr)
            mpv_terminate_destroy(mpv);
    }

    /// In a thread of libmpv: no mpv call here, only wake the main loop (SDL_PushEvent is thread-safe).
    void MpvMedia::wake(void *self)
    {
        auto *me = static_cast<MpvMedia *>(self);
        if (me->context.redraw)
            me->context.redraw();
    }

    void MpvMedia::command(std::initializer_list<const char *> args)
    {
        std::vector<const char *> list(args);
        list.push_back(nullptr);
        mpv_command_async(mpv, 0, list.data());
    }

    void MpvMedia::start()
    {
        mpv = mpv_create();
        if (mpv == nullptr)
        {
            report(Outcome::Failed, "libmpv cannot start for " + source);
            return;
        }
        mpv_set_option_string(mpv, "vo", "libmpv");
        // hardware decoding: first the V4L2 decoder of a Raspberry Pi (H.264 up to 1080p) without copy, the frames
        // as GPU buffers (EGL dmabuf); auto-safe does not take it, decoded by the CPU a 1080p H.264 cost more than one
        // core of a Pi 4, with v4l2m2m-copy about 70 %, without copy about 40 %. Without a V4L2 decoder mpv goes on
        // with a safe way of the device. GARLIC_MPV_HWDEC overrides it (e.g. "no", "auto-copy-safe") for a device
        // whose driver fails with it
        const char *hwdec = std::getenv("GARLIC_MPV_HWDEC");
        mpv_set_option_string(mpv, "hwdec", hwdec != nullptr && *hwdec != '\0' ? hwdec : "v4l2m2m,auto-safe");
        mpv_set_option_string(mpv, "load-scripts", "no"); // no osc, stats, console …
        mpv_set_option_string(mpv, "audio-client-name", "garlic-player");
        mpv_set_option_string(mpv, "idle", "yes"); // stays after the end, for repeat()
        mpv_set_option_string(mpv, "keep-open", "no");
        mpv_set_option_string(mpv, "terminal", "no");
        mpv_set_option_string(mpv, "input-default-bindings", "no");
        mpv_set_option_string(mpv, "osc", "no");
        mpv_set_option_string(mpv, "ytdl", "no");
        mpv_set_option_string(mpv, "audio-display", "no");
        if (!is_video)
            mpv_set_option_string(mpv, "vid", "no");
        // fit and mediaAlign like images (place()): the texture gets the size and position of the video in the region,
        // mpv fills it completely; the window cuts what lies outside the region (slice)
        mpv_set_option_string(mpv, "keepaspect", "no");
        mpv_set_option_string(mpv, "volume", std::to_string(std::lround(volume() * 100)).c_str());
        if (mpv_initialize(mpv) < 0)
        {
            report(Outcome::Failed, "libmpv cannot start for " + source);
            return;
        }
        mpv_set_wakeup_callback(mpv, &MpvMedia::wake, this);

        if (is_video)
        {
            if (context.renderer == nullptr || !isOpenGl(*context.renderer))
            {
                report(Outcome::Failed, "video needs the OpenGL renderer: " + source);
                return;
            }
            if (!ensureMpvContext(*context.renderer))
            {
                report(Outcome::Failed, "no OpenGL context for libmpv: " + source);
                return;
            }
            // the display connection lets mpv hand hardware decoded frames over without a copy (e.g. VA-API)
            SDL_PropertiesID       window_properties = SDL_GetWindowProperties(mpvGl().window);
            void *wayland = SDL_GetPointerProperty(window_properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
            void *x11     = SDL_GetPointerProperty(window_properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
            mpv_opengl_init_params init{&procAddress, nullptr};
            const char            *api = MPV_RENDER_API_TYPE_OPENGL;
            std::vector<mpv_render_param> params{{MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(api)},
                                                 {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &init}};
            if (wayland != nullptr)
                params.push_back({MPV_RENDER_PARAM_WL_DISPLAY, wayland});
            else if (x11 != nullptr)
                params.push_back({MPV_RENDER_PARAM_X11_DISPLAY, x11});
            params.push_back({MPV_RENDER_PARAM_INVALID, nullptr});
            int created = 0;
            {
                InMpvContext in_mpv(*context.renderer);
                created = mpv_render_context_create(&render, mpv, params.data());
            }
            if (created < 0)
            {
                render = nullptr;
                report(Outcome::Failed, "libmpv cannot render video: " + source);
                return;
            }
            mpv_render_context_set_update_callback(render, &MpvMedia::wake, this);
        }
        playing = true;
        command({"loadfile", source.c_str()});
    }

    void MpvMedia::stop()
    {
        playing = false;
        if (mpv != nullptr)
            command({"stop"});
    }

    void MpvMedia::pause()
    {
        if (mpv != nullptr)
            mpv_set_property_string(mpv, "pause", "yes");
    }

    void MpvMedia::resume()
    {
        if (mpv != nullptr)
            mpv_set_property_string(mpv, "pause", "no");
    }

    /// RepeatMedia: again from the beginning
    void MpvMedia::repeat()
    {
        if (mpv == nullptr)
            return;
        playing = true;
        mpv_set_property_string(mpv, "pause", "no");
        command({"loadfile", source.c_str(), "replace"});
    }

    void MpvMedia::update()
    {
        if (mpv == nullptr)
            return;
        if (render != nullptr && (mpv_render_context_update(render) & MPV_RENDER_UPDATE_FRAME) != 0)
            new_frame = true;
        while (true)
        {
            mpv_event *event = mpv_wait_event(mpv, 0);
            if (event->event_id == MPV_EVENT_NONE)
                break;
            if (event->event_id == MPV_EVENT_VIDEO_RECONFIG)
            {
                // the size of the video as shown (rotation and pixel aspect applied), for fit and mediaAlign
                std::int64_t width  = 0;
                std::int64_t height = 0;
                mpv_get_property(mpv, "dwidth", MPV_FORMAT_INT64, &width);
                mpv_get_property(mpv, "dheight", MPV_FORMAT_INT64, &height);
                video_width  = static_cast<int>(width);
                video_height = static_cast<int>(height);
                new_frame    = true;
                continue;
            }
            if (event->event_id != MPV_EVENT_END_FILE || !playing)
                continue;
            auto *end = static_cast<mpv_event_end_file *>(event->data);
            if (end->reason == MPV_END_FILE_REASON_EOF)
            {
                playing = false;
                report(Outcome::Finished, {});
            }
            else if (end->reason == MPV_END_FILE_REASON_ERROR)
            {
                playing = false;
                report(Outcome::Failed, std::string("libmpv: ") + mpv_error_string(end->error) + " " + source);
            }
        }
    }

    /// A texture of the renderer in the size the video is drawn, and a framebuffer on it for mpv
    bool MpvMedia::prepareFrame(int width, int height)
    {
        if (texture != nullptr && width == frame_width && height == frame_height)
            return true;
        releaseFrame();
        if (width <= 0 || height <= 0 || !gl().genFramebuffers)
            return false;
        texture = SDL_CreateTexture(context.renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, width, height);
        if (texture == nullptr)
            return false;
        SDL_PropertiesID properties = SDL_GetTextureProperties(texture);
        auto id = static_cast<GLuint>(SDL_GetNumberProperty(properties, SDL_PROP_TEXTURE_OPENGL_TEXTURE_NUMBER, 0));
        auto target = static_cast<GLenum>(
            SDL_GetNumberProperty(properties, SDL_PROP_TEXTURE_OPENGL_TEXTURE_TARGET_NUMBER, GL_TEXTURE_2D));
        if (id == 0) // OpenGL ES
        {
            id     = static_cast<GLuint>(SDL_GetNumberProperty(properties, SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_NUMBER, 0));
            target = static_cast<GLenum>(
                SDL_GetNumberProperty(properties, SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_TARGET_NUMBER, GL_TEXTURE_2D));
        }
        if (id == 0)
        {
            releaseFrame();
            return false;
        }

        // framebuffers are not shared between contexts: this one belongs to the context of mpv, the texture to both
        GLuint created  = 0;
        bool   complete = false;
        {
            InMpvContext in_mpv(*context.renderer);
            gl().genFramebuffers(1, &created);
            gl().bindFramebuffer(GL_FRAMEBUFFER, created);
            gl().framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, id, 0);
            complete = gl().checkFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
            gl().bindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        framebuffer  = created;
        frame_width  = width;
        frame_height = height;
        if (!complete)
        {
            releaseFrame();
            return false;
        }
        new_frame = true; // the new size needs a frame even while paused
        return true;
    }

    void MpvMedia::releaseFrame()
    {
        if (framebuffer != 0)
        {
            InMpvContext in_mpv(*context.renderer);
            GLuint       name = framebuffer;
            gl().deleteFramebuffers(1, &name);
            framebuffer = 0;
        }
        if (texture != nullptr)
            SDL_DestroyTexture(texture);
        texture      = nullptr;
        frame_width  = 0;
        frame_height = 0;
    }

    void MpvMedia::draw(SDL_Renderer &target, const SDL_FRect &area)
    {
        if (render == nullptr || video_width <= 0 || video_height <= 0) // no picture yet
            return;
        SDL_FRect where = place(area, static_cast<float>(video_width), static_cast<float>(video_height), fit(),
                                mediaAlign());
        if (!prepareFrame(std::max(1, static_cast<int>(std::lround(where.w))),
                          std::max(1, static_cast<int>(std::lround(where.h)))))
            return;
        if (new_frame)
        {
            void *fence = nullptr;
            {
                InMpvContext     in_mpv(target);
                mpv_opengl_fbo   fbo{static_cast<int>(framebuffer), frame_width, frame_height, 0};
                int              flip     = 0;
                mpv_render_param params[] = {{MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
                                             {MPV_RENDER_PARAM_FLIP_Y, &flip},
                                             {MPV_RENDER_PARAM_INVALID, nullptr}};
                mpv_render_context_render(render, params);
                // SDL may read the texture only when mpv has written it
                if (mpvGl().has_sync)
                {
                    fence = gl().fenceSync(sync_gpu_commands_complete, 0);
                    gl().flush();
                }
                else
                    gl().finish();
            }
            if (fence != nullptr)
            {
                gl().waitSync(fence, 0, timeout_ignored); // in the context of SDL, the GPU waits, not the CPU
                gl().deleteSync(fence);
            }
            new_frame = false;
        }
        SDL_RenderTexture(&target, texture, nullptr, &where);
    }
}
