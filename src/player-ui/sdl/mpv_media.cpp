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
         * libmpv 0.40 and 0.41 (render API; mpv issue 17217, fixed in 0.42) make a fence for every frame and delete
         * it only when swapping buffers, which the render API never does. On a Raspberry Pi (Mesa V3D) every fence
         * holds a sync_file, so after about a minute the player ran out of file descriptors (1024) and showed nothing
         * anymore. Without glFenceSync mpv refuses OpenGL 3.2 and OpenGL ES 3.0 (a required function), so mpv gets
         * it through trackFence(), which remembers the last fence: the one of the frame, made at the end of
         * mpv_render_context_render(); draw() deletes it with these versions.
         */
        struct FenceTracking
        {
                void *(*real)(GLenum, GLbitfield) = nullptr;
                void *last_fence                   = nullptr; // of the current mpv_render_context_render()
                bool  leaky                        = false;   // mpv 0.40 or 0.41
        };

        FenceTracking &fences()
        {
            static FenceTracking tracking;
            return tracking;
        }

        void *trackFence(GLenum condition, GLbitfield flags)
        {
            FenceTracking &t = fences();
            t.last_fence     = t.real(condition, flags);
            return t.last_fence;
        }

        /// GARLIC_MPV_FENCES=0: mpv gets no glFenceSync (as before 2.0.0-dev.968), for a test of the memory on a
        /// Raspberry Pi; mpv then takes OpenGL (ES) only in the scope of 3.1 / 2.0
        bool fencesForMpv()
        {
            const char *value = std::getenv("GARLIC_MPV_FENCES");
            return value == nullptr || std::string_view(value) != "0";
        }

        void *procAddress(void * /*context*/, const char *name)
        {
            void *function = reinterpret_cast<void *>(SDL_GL_GetProcAddress(name));
            if (function != nullptr && std::string_view(name) == "glFenceSync")
            {
                static const bool given = fencesForMpv();
                if (!given)
                    return nullptr;
                fences().real = reinterpret_cast<void *(*)(GLenum, GLbitfield)>(function);
                return reinterpret_cast<void *>(&trackFence);
            }
            return function;
        }

        /// "mpv 0.40.0", "mpv v0.41.0-…": the versions whose render API leaks a fence per frame
        bool leaksFences(mpv_handle *mpv)
        {
            char *version = mpv_get_property_string(mpv, "mpv-version");
            if (version == nullptr)
                return false;
            std::string text(version);
            mpv_free(version);
            return text.find("0.40.") != std::string::npos || text.find("0.41.") != std::string::npos;
        }
    }

    namespace
    {
        VideoQuality video_quality = VideoQuality::Auto;

        /// The GPU of a Raspberry Pi (OpenGL renderer "V3D 4.2" Pi 4, "V3D 7.1" Pi 5, "VC4 V3D" Pi 3), read once from
        /// the context of the renderer, which is the current one in the main thread
        bool raspberryGpu()
        {
            static const bool raspberry = []
            {
                auto get_string = reinterpret_cast<const unsigned char *(*)(GLenum)>(SDL_GL_GetProcAddress("glGetString"));
                const unsigned char *name = get_string != nullptr ? get_string(GL_RENDERER) : nullptr;
                return name != nullptr && std::string(reinterpret_cast<const char *>(name)).find("V3D") != std::string::npos;
            }();
            return raspberry;
        }

        /// Auto: low on a Raspberry Pi
        VideoQuality effectiveQuality()
        {
            if (video_quality != VideoQuality::Auto)
                return video_quality;
            return raspberryGpu() ? VideoQuality::Low : VideoQuality::Normal;
        }
    }

    void setVideoQuality(VideoQuality quality)
    {
        video_quality = quality;
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
        // low: the cheap scalers of mpv (profile fast: bilinear, no dither, no deband); with the default ones a 4K
        // video stuttered on a Raspberry Pi 4, its GPU converts and scales every frame. high: profile high-quality
        // (gpu-hq before mpv 0.37)
        if (is_video)
        {
            VideoQuality quality = effectiveQuality();
            if (quality == VideoQuality::Low)
                mpv_set_option_string(mpv, "profile", "fast");
            else if (quality == VideoQuality::High && mpv_set_option_string(mpv, "profile", "high-quality") < 0)
                mpv_set_option_string(mpv, "profile", "gpu-hq");
        }
        // hardware decoding of a Raspberry Pi without copy, the frames as GPU buffers (EGL dmabuf), which auto-safe
        // does not take: v4l2m2m for H.264 up to 1080p (decoded by the CPU a 1080p H.264 cost more than one core of a
        // Pi 4, with v4l2m2m-copy about 70 %, without copy about 40 %), drm for H.265 up to 4K (the FFmpeg of
        // Raspberry Pi OS; a 4K60 10 bit H.265 decodes at 75 fps without copy, 22 fps with). Without them mpv goes
        // on with a safe way of the device. GARLIC_MPV_HWDEC overrides it (e.g. "no", "auto-copy-safe") for a device
        // whose driver fails with it
        const char *hwdec = std::getenv("GARLIC_MPV_HWDEC");
        mpv_set_option_string(mpv, "hwdec", hwdec != nullptr && *hwdec != '\0' ? hwdec : "v4l2m2m,drm,auto-safe");
        // Raspberry Pi: only the interop of v4l2m2m and drm (GPU buffers as EGL dmabuf); with auto mpv loaded every
        // interop for each video, also the one of CUDA ("Cannot load libcuda.so.1" in the log for each video)
        if (is_video && raspberryGpu())
            mpv_set_option_string(mpv, "gpu-hwdec-interop", "drmprime");
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
        fences().leaky = leaksFences(mpv); // properties only after mpv_initialize(), before it mpv-version is null
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
                fences().last_fence = nullptr;
                mpv_render_context_render(render, params);
                if (fences().leaky && fences().last_fence != nullptr) // the fence of the frame, never deleted by mpv
                    gl().deleteSync(fences().last_fence);
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
