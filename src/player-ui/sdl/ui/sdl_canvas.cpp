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
#include "sdl_canvas.hpp"

#include <SDL3/SDL_error.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace player::ui
{
    namespace
    {
        constexpr std::string_view ellipsis = "…";
    }

    std::expected<std::unique_ptr<SdlCanvas>, std::string> SdlCanvas::open(SDL_Renderer &renderer,
                                                                          const std::string &font_file)
    {
        TTF_Font *font = TTF_OpenFont(font_file.c_str(), font_points);
        if (font == nullptr)
            return std::unexpected("cannot open font " + font_file + ": " + SDL_GetError());
        TTF_TextEngine *engine = TTF_CreateRendererTextEngine(&renderer);
        if (engine == nullptr)
        {
            TTF_CloseFont(font);
            return std::unexpected(std::string("cannot create text engine: ") + SDL_GetError());
        }
        return std::unique_ptr<SdlCanvas>(new SdlCanvas(renderer, engine, font));
    }

    SdlCanvas::SdlCanvas(SDL_Renderer &r, TTF_TextEngine *e, TTF_Font *f) noexcept : renderer(&r), engine(e), font(f)
    {
    }

    SdlCanvas::~SdlCanvas()
    {
        clearCache(); // the shaped lines before the engine and the font they come from
        TTF_DestroyRendererTextEngine(engine);
        TTF_CloseFont(font);
    }

    void SdlCanvas::setScale(float pixels_per_point)
    {
        // rounded, so a window being dragged to size does not make a font for every pixel
        pixels_per_point = std::max(0.25F, std::round(pixels_per_point * 20.0F) / 20.0F);
        if (pixels_per_point == density)
            return;
        density = pixels_per_point;
        clearCache();
        TTF_SetFontSize(font, font_points * density);
    }

    void SdlCanvas::clearCache()
    {
        for (const auto &[key, shaped] : cache)
            TTF_DestroyText(shaped);
        cache.clear();
        widths.clear();
    }

    TTF_Text *SdlCanvas::line(std::string_view text)
    {
        std::string key(text);
        if (auto found = cache.find(key); found != cache.end())
            return found->second;
        if (cache.size() >= cache_limit)
            clearCache();
        TTF_Text *created = TTF_CreateText(engine, font, key.data(), key.size());
        if (created != nullptr)
            cache.emplace(std::move(key), created);
        return created;
    }

    void SdlCanvas::fillRect(const Rect &rect, Color color)
    {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue, color.alpha);
        SDL_FRect area{rect.x * density, rect.y * density, rect.width * density, rect.height * density};
        SDL_RenderFillRect(renderer, &area);
    }

    void SdlCanvas::fillRoundedRect(const Rect &rect, float radius, Color top, Color bottom)
    {
        if (rect.width <= 0.0F || rect.height <= 0.0F)
            return;
        radius = std::clamp(radius, 0.0F, std::min(rect.width, rect.height) / 2);

        // the outline: four corner arcs, clockwise from the top left corner's start
        constexpr int   arc_steps = 6;
        constexpr float half_pi   = 1.5707963F;
        std::vector<Point>          outline;
        std::array<Point, 4> centers{{{rect.x + radius, rect.y + radius},
                                      {rect.x + rect.width - radius, rect.y + radius},
                                      {rect.x + rect.width - radius, rect.y + rect.height - radius},
                                      {rect.x + radius, rect.y + rect.height - radius}}};
        for (int corner = 0; corner < 4; ++corner)
        {
            float start = (static_cast<float>(corner) + 2.0F) * half_pi; // 180, 270, 0, 90 degrees
            for (int step = 0; step <= arc_steps; ++step)
            {
                float angle = start + (half_pi * static_cast<float>(step) / arc_steps);
                outline.push_back({centers[static_cast<std::size_t>(corner)].x + (radius * SDL_cosf(angle)),
                                   centers[static_cast<std::size_t>(corner)].y + (radius * SDL_sinf(angle))});
            }
        }

        // a fan around the center; the color follows y, so the gradient is exact
        auto color_at = [&](float y)
        {
            float t   = std::clamp((y - rect.y) / rect.height, 0.0F, 1.0F);
            auto  mix = [t](unsigned char from, unsigned char to)
            { return static_cast<float>(from) + (static_cast<float>(to) - static_cast<float>(from)) * t; };
            return SDL_FColor{mix(top.red, bottom.red) / 255.0F, mix(top.green, bottom.green) / 255.0F,
                              mix(top.blue, bottom.blue) / 255.0F, mix(top.alpha, bottom.alpha) / 255.0F};
        };
        std::vector<SDL_Vertex> vertices;
        float                   center_y = rect.y + (rect.height / 2);
        vertices.push_back({{(rect.x + (rect.width / 2)) * density, center_y * density}, color_at(center_y), {0, 0}});
        for (const Point &point : outline)
            vertices.push_back({{point.x * density, point.y * density}, color_at(point.y), {0, 0}});
        std::vector<int> indices;
        for (std::size_t index = 0; index < outline.size(); ++index)
        {
            indices.push_back(0);
            indices.push_back(static_cast<int>(index + 1));
            indices.push_back(static_cast<int>(((index + 1) % outline.size()) + 1));
        }
        SDL_RenderGeometry(renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()), indices.data(),
                           static_cast<int>(indices.size()));
    }

    void SdlCanvas::text(Point position, std::string_view text, Color color)
    {
        if (text.empty())
            return;
        TTF_Text *shaped = line(text);
        if (shaped == nullptr)
            return;
        TTF_SetTextColor(shaped, color.red, color.green, color.blue, color.alpha);
        TTF_DrawRendererText(shaped, SDL_roundf(position.x * density), SDL_roundf(position.y * density));
    }

    void SdlCanvas::textElided(Point position, std::string_view text, Color color, float max_width)
    {
        if (textWidth(text) * density <= max_width * density + 0.5F)
        {
            this->text(position, text, color);
            return;
        }
        float room = (max_width - textWidth(ellipsis)) * density;
        if (room <= 0.0F)
            return;
        int         measured_width  = 0;
        std::size_t measured_length = 0;
        TTF_MeasureString(font, text.data(), text.size(), static_cast<int>(room), &measured_width, &measured_length);
        this->text(position, std::string(text.substr(0, measured_length)) + std::string(ellipsis), color);
    }

    float SdlCanvas::textHeight() const noexcept
    {
        return static_cast<float>(TTF_GetFontHeight(font)) / density;
    }

    float SdlCanvas::textWidth(std::string_view text) const noexcept
    {
        if (text.empty()) // SDL_ttf compares an empty text through a null pointer
            return 0.0F;
        std::string key(text);
        if (auto found = widths.find(key); found != widths.end())
            return found->second;
        int pixels = 0;
        int height = 0;
        TTF_GetStringSize(font, key.data(), key.size(), &pixels, &height);
        float points = static_cast<float>(pixels) / density;
        if (widths.size() < cache_limit)
            widths.emplace(std::move(key), points);
        return points;
    }

    void SdlCanvas::setClip(std::optional<Rect> clip)
    {
        if (!clip)
        {
            SDL_SetRenderClipRect(renderer, nullptr);
            return;
        }
        SDL_Rect area{static_cast<int>(SDL_floorf(clip->x * density)), static_cast<int>(SDL_floorf(clip->y * density)),
                      static_cast<int>(SDL_ceilf(clip->width * density)),
                      static_cast<int>(SDL_ceilf(clip->height * density))};
        SDL_SetRenderClipRect(renderer, &area);
    }
}
