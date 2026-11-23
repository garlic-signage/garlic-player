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
#ifndef PLAYER_UI_SDL_MPV_MEDIA_HPP
#define PLAYER_UI_SDL_MPV_MEDIA_HPP

#include "media.hpp"

struct mpv_handle;
struct mpv_render_context;

namespace player
{
    /// Frees the OpenGL context of libmpv; before the renderer goes, after the last medium.
    void shutdownMpv();

    /// How mpv scales and filters video (--video-quality): mpv's profiles fast and high-quality, or its defaults
    enum class VideoQuality
    {
        Auto,   // low on the graphics of a Raspberry Pi (V3D), normal elsewhere
        Low,    // mpv's profile fast (bilinear, no dither, no deband): 4K at 25/30 fps on a Raspberry Pi 4
        Normal, // the defaults of mpv
        High    // mpv's profile high-quality, for a strong GPU
    };

    /// Before the first video
    void setVideoQuality(VideoQuality quality);

    /**
     * <video> and <audio> with libmpv, one player per medium. Video renders
     * with the render API (OpenGL, in a context of its own that shares the texture) into a texture of the SDL renderer, which the window draws like an image, so it
     * lies in its region under everything with a higher z-index. Decoding in hardware where mpv finds a way
     * (hwdec=auto-safe).
     *
     * Only the render API of libmpv 0.35 (client API 2.0) is used, the oldest version for Debian.
     */
    class MpvMedia final : public Media
    {
        public:
            MpvMedia(const garlic::core::smil::Media &smil, const garlic::core::smil::Region &region, std::string source,
                     Report report, const MediaContext &context, bool video);
            ~MpvMedia() override;

            [[nodiscard]] bool visible() const override { return is_video; }

            void start() override;
            void stop() override;
            void pause() override;
            void resume() override;
            void repeat() override;
            void update() override;
            void draw(SDL_Renderer &renderer, const SDL_FRect &area) override;

        private:
            MediaContext        context;
            bool                is_video;
            mpv_handle         *mpv    = nullptr;
            mpv_render_context *render = nullptr;
            bool                playing = false; // between start() and the end; an end after stop() is no end
            bool                started = false; // played since the last loadfile: an error now is a playback error
            bool                new_frame = false;
            int                 video_width  = 0; // as shown, from mpv (dwidth, dheight); 0 until it knows
            int                 video_height = 0;

            // the frame: a texture of the renderer and a framebuffer of ours on it
            SDL_Texture *texture     = nullptr;
            unsigned     framebuffer = 0;
            int          frame_width  = 0;
            int          frame_height = 0;

            void command(std::initializer_list<const char *> args);
            bool prepareFrame(int width, int height);
            void releaseFrame();
            static void wake(void *self);
    };
}

#endif // PLAYER_UI_SDL_MPV_MEDIA_HPP
