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
#include "media.hpp"

#include "color.hpp"
#include "file_url.hpp"
#include "web_view.hpp"

#ifdef PLAYER_SDL_WITH_MPV
    #include "mpv_media.hpp"
#endif

#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>

namespace player
{
    using garlic::core::smil::MediaKind;

    namespace
    {
        std::string lower(std::string text)
        {
            std::ranges::transform(text, text.begin(), [](unsigned char c) { return std::tolower(c); });
            return text;
        }

        /// Read from the data, not by the name: sometimes a png is called jpg or the other way round (garlic-lib).
        class Image final : public Media
        {
            public:
                Image(const garlic::core::smil::Media &s, const garlic::core::smil::Region &r, std::string src, Report rep,
                      SDL_Renderer *renderer)
                    : Media(s, r, std::move(src), std::move(rep)), renderer(renderer)
                {
                }

                ~Image() override
                {
                    if (texture != nullptr)
                        SDL_DestroyTexture(texture);
                    if (image != nullptr)
                        SDL_DestroySurface(image);
                }

                void start() override
                {
                    SDL_Surface *loaded = IMG_Load(source.c_str());
                    if (loaded == nullptr)
                    {
                        report(Outcome::Failed, "cannot load image " + source + ": " + SDL_GetError());
                        return;
                    }
                    image = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_ARGB8888); // linear scaling needs 32 bit
                    SDL_DestroySurface(loaded);
                    if (image == nullptr)
                        report(Outcome::Failed, "cannot convert image " + source + ": " + SDL_GetError());
                }

                void draw(SDL_Renderer &target, const SDL_FRect &area) override
                {
                    if (image == nullptr)
                        return;
                    SDL_FRect where  = place(area, static_cast<float>(image->w), static_cast<float>(image->h), fit(),
                                             mediaAlign());
                    int       width  = std::max(1, static_cast<int>(std::lround(where.w)));
                    int       height = std::max(1, static_cast<int>(std::lround(where.h)));
                    if (texture == nullptr || width != texture_width || height != texture_height)
                        makeTexture(width, height);
                    if (texture != nullptr)
                        SDL_RenderTexture(&target, texture, nullptr, &where);
                }

            private:
                SDL_Renderer *renderer;
                SDL_Surface  *image          = nullptr; // the original, kept for a new size of the region
                SDL_Texture  *texture        = nullptr; // in the size it is drawn
                int           texture_width  = 0;
                int           texture_height = 0;

                /**
                 * The texture in the size it is drawn, like the qwidget player (Qt::SmoothTransformation at every size).
                 * The GPU scales linearly from the 4 nearest pixels only: shrunk to a third or less, edges fray. So the
                 * image is halved step by step, which averages all pixels like a mipmap, and scaled to the exact size
                 * last. Enlarged it stays the original; the GPU scales it up.
                 */
                void makeTexture(int width, int height)
                {
                    if (texture != nullptr)
                        SDL_DestroyTexture(texture);
                    texture        = nullptr;
                    texture_width  = width;
                    texture_height = height;

                    SDL_Surface *current = image;
                    if (width < image->w || height < image->h)
                    {
                        while (current->w > 2 * width || current->h > 2 * height)
                        {
                            SDL_Surface *half = SDL_ScaleSurface(current, std::max(width, current->w / 2),
                                                                 std::max(height, current->h / 2), SDL_SCALEMODE_LINEAR);
                            if (current != image)
                                SDL_DestroySurface(current);
                            current = half;
                            if (current == nullptr)
                                break;
                        }
                        if (current != nullptr)
                        {
                            SDL_Surface *exact = SDL_ScaleSurface(current, width, height, SDL_SCALEMODE_LINEAR);
                            if (current != image)
                                SDL_DestroySurface(current);
                            current = exact;
                        }
                    }
                    if (current == nullptr) // scaling failed: the original, the GPU scales it
                        current = image;
                    texture = SDL_CreateTextureFromSurface(renderer, current);
                    if (current != image)
                        SDL_DestroySurface(current);
                    if (texture != nullptr)
                        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
                }
        };

        /// <brush color="…">: a colored area.
        class Brush final : public Media
        {
            public:
                using Media::Media;

                void start() override { color = parseColor(smil.color); }

                void draw(SDL_Renderer &target, const SDL_FRect &area) override
                {
                    if (!color)
                        return;
                    SDL_SetRenderDrawBlendMode(&target, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(&target, color->red, color->green, color->blue, color->alpha);
                    SDL_RenderFillRect(&target, &area);
                }

            private:
                std::optional<ui::Color> color;
        };

        /// <ref type="text/html">, <text>, widgets: through the WebView of the platform
        class Web final : public Media
        {
            public:
                Web(const garlic::core::smil::Media &s, const garlic::core::smil::Region &r, std::string src, Report rep,
                    std::unique_ptr<WebView> web)
                    : Media(s, r, std::move(src), std::move(rep)), view(std::move(web))
                {
                }

                void start() override { view->load(fileUrl(source)); }
                void stop() override { view->stop(); }
                void draw(SDL_Renderer &target, const SDL_FRect &area) override { view->draw(target, area); }

                [[nodiscard]] bool interactive() const override { return true; }
                void input(const SDL_Event &event, float x, float y) override { view->input(event, x, y); }

            private:
                std::unique_ptr<WebView> view;
        };

        /**
         * What cannot be played: <ref src="adapi:…"> other than adapi:blankScreen, which the window handles
         * (garlic-lib knew no other command), and kinds this build has no library for. The core goes on.
         */
        class NotPlayable final : public Media
        {
            public:
                NotPlayable(const garlic::core::smil::Media &s, const garlic::core::smil::Region &r, std::string src,
                            Report rep, std::string why)
                    : Media(s, r, std::move(src), std::move(rep)), reason(std::move(why))
                {
                }

                [[nodiscard]] bool visible() const override { return false; }
                void               start() override { report(Outcome::Failed, reason + " " + source); }
                void               draw(SDL_Renderer & /*target*/, const SDL_FRect & /*area*/) override {}

            private:
                std::string reason;
        };
    }

    Media::Media(const garlic::core::smil::Media &s, const garlic::core::smil::Region &r, std::string src, Report rep)
        : smil(s), region(r), source(std::move(src)), report(std::move(rep))
    {
    }

    std::string Media::fit() const
    {
        return lower(smil.fit.empty() ? region.fit : smil.fit);
    }

    std::string Media::mediaAlign() const
    {
        return lower(smil.media_align.empty() ? region.media_align : smil.media_align);
    }

    double Media::volume() const
    {
        const std::string &level = smil.sound_level.empty() ? region.sound_level : smil.sound_level;
        if (!level.ends_with('%'))
            return 1;
        double percent    = 0;
        auto [end, error] = std::from_chars(level.data(), level.data() + level.size() - 1, percent);
        if (error != std::errc{} || end != level.data() + level.size() - 1)
            return 1;
        return std::clamp(percent / 100, 0.0, 1.0);
    }

    std::unique_ptr<Media> createMedia(const garlic::core::smil::Media &smil, const garlic::core::smil::Region &region,
                                       const std::string &source, Report report, const MediaContext &context)
    {
        switch (smil.kind)
        {
            case MediaKind::Image:
                return std::make_unique<Image>(smil, region, source, std::move(report), context.renderer);
            case MediaKind::Video:
            case MediaKind::Audio:
#ifdef PLAYER_SDL_WITH_MPV
                return std::make_unique<MpvMedia>(smil, region, source, std::move(report), context,
                                                  smil.kind == MediaKind::Video);
#else
                return std::make_unique<NotPlayable>(smil, region, source, std::move(report),
                                                     "no video and audio in this build (libmpv):");
#endif
            case MediaKind::Web:
            case MediaKind::Widget:
                if (auto view = createWebView(context))
                    return std::make_unique<Web>(smil, region, source, std::move(report), std::move(view));
                return std::make_unique<NotPlayable>(smil, region, source, std::move(report),
                                                     "no web pages in this build:");
            case MediaKind::Brush: return std::make_unique<Brush>(smil, region, source, std::move(report));
            case MediaKind::Command:
                return std::make_unique<NotPlayable>(smil, region, source, std::move(report), "unknown command");
            case MediaKind::Prefetch: return nullptr;
        }
        return nullptr;
    }
}
