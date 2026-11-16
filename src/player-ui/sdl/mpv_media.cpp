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

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace player
{
    namespace
    {
        /// The few OpenGL (ES 2) functions for the framebuffer, from the context of the SDL renderer
        struct Gl
        {
                void (*genFramebuffers)(GLsizei, GLuint *)                             = nullptr;
                void (*deleteFramebuffers)(GLsizei, const GLuint *)                    = nullptr;
                void (*bindFramebuffer)(GLenum, GLuint)                                = nullptr;
                void (*framebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint)    = nullptr;
                GLenum (*checkFramebufferStatus)(GLenum)                               = nullptr;
                void (*getIntegerv)(GLenum, GLint *)                                   = nullptr;
                void (*viewport)(GLint, GLint, GLsizei, GLsizei)                       = nullptr;

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
                    get(viewport, "glViewport");
                    return genFramebuffers && deleteFramebuffers && bindFramebuffer && framebufferTexture2D &&
                           checkFramebufferStatus && getIntegerv && viewport;
                }
        };

        Gl &gl()
        {
            static Gl functions;
            static bool loaded = functions.load();
            (void)loaded;
            return functions;
        }

        bool isOpenGl(SDL_Renderer &renderer)
        {
            std::string name = SDL_GetRendererName(&renderer);
            return name == "opengl" || name == "opengles2";
        }

        void *procAddress(void * /*context*/, const char *name)
        {
            return reinterpret_cast<void *>(SDL_GL_GetProcAddress(name));
        }
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
            SDL_FlushRenderer(context.renderer); // its OpenGL context is the current one
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
        std::string how = fit();
        mpv_set_option_string(mpv, "vo", "libmpv");
        // hardware decoding where mpv finds a safe way; GARLIC_MPV_HWDEC overrides it (e.g. "no", "auto-copy-safe")
        // for a device whose driver fails with it
        const char *hwdec = std::getenv("GARLIC_MPV_HWDEC");
        mpv_set_option_string(mpv, "hwdec", hwdec != nullptr && *hwdec != '\0' ? hwdec : "auto-safe");
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
        // fit like the qwidget player: fill stretches, slice covers, everything else keeps the aspect inside
        mpv_set_option_string(mpv, "keepaspect", how == "fill" ? "no" : "yes");
        mpv_set_option_string(mpv, "panscan", how == "slice" ? "1.0" : "0.0");
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
            SDL_FlushRenderer(context.renderer);
            mpv_opengl_init_params init{&procAddress, nullptr};
            const char            *api = MPV_RENDER_API_TYPE_OPENGL;
            mpv_render_param       params[] = {{MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(api)},
                                               {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &init},
                                               {MPV_RENDER_PARAM_INVALID, nullptr}};
            if (mpv_render_context_create(&render, mpv, params) < 0)
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

    /// A texture of the renderer in the size of the region, and a framebuffer on it for mpv
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

        SDL_FlushRenderer(context.renderer);
        GLint previous = 0;
        gl().getIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
        GLuint created = 0;
        gl().genFramebuffers(1, &created);
        gl().bindFramebuffer(GL_FRAMEBUFFER, created);
        gl().framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, id, 0);
        bool complete = gl().checkFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        gl().bindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
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
            SDL_FlushRenderer(context.renderer);
            GLuint name = framebuffer;
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
        if (render == nullptr)
            return;
        if (!prepareFrame(static_cast<int>(std::lround(area.w)), static_cast<int>(std::lround(area.h))))
            return;
        if (new_frame)
        {
            // OpenGL between the commands of SDL: flushed before, and SDL sets its state anew afterwards
            SDL_FlushRenderer(&target);
            GLint previous = 0;
            GLint viewport[4]{};
            gl().getIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
            gl().getIntegerv(GL_VIEWPORT, viewport);
            mpv_opengl_fbo   fbo{static_cast<int>(framebuffer), frame_width, frame_height, 0};
            int              flip     = 0;
            mpv_render_param params[] = {{MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
                                         {MPV_RENDER_PARAM_FLIP_Y, &flip},
                                         {MPV_RENDER_PARAM_INVALID, nullptr}};
            mpv_render_context_render(render, params);
            gl().bindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
            gl().viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            new_frame = false;
        }
        SDL_RenderTexture(&target, texture, nullptr, &area);
    }
}
