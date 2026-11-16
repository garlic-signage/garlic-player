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
#ifndef PLAYER_UI_SDL_UI_SDL_CANVAS_HPP
#define PLAYER_UI_SDL_UI_SDL_CANVAS_HPP

#include "canvas.hpp"

#include <SDL3/SDL_render.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <expected>
#include <memory>
#include <string>
#include <unordered_map>

namespace player::ui
{
    /**
     * Canvas on an SDL renderer, with text by SDL3_ttf (copied from the DJ workstation: SdlCanvas and SdlText, with
     * one font size). The widgets work in points, the renderer in pixels; scale (pixels per point) converts between
     * them, so the menu and the dialogs keep their size relative to the screen.
     */
    class SdlCanvas final : public Canvas
    {
        public:
            static constexpr float font_points = 16.0F;

            static std::expected<std::unique_ptr<SdlCanvas>, std::string> open(SDL_Renderer &renderer,
                                                                              const std::string &font_file);
            ~SdlCanvas() override;

            /// Pixels per point; the font is made anew when it changes
            void                setScale(float pixels_per_point);
            [[nodiscard]] float scale() const noexcept { return density; }

            void fillRect(const Rect &rect, Color color) override;
            void fillRoundedRect(const Rect &rect, float radius, Color top, Color bottom) override;
            void text(Point position, std::string_view text, Color color) override;
            void textElided(Point position, std::string_view text, Color color, float max_width) override;
            [[nodiscard]] float textHeight() const noexcept override;
            [[nodiscard]] float textWidth(std::string_view text) const noexcept override;
            void                setClip(std::optional<Rect> clip) override;

        private:
            SdlCanvas(SDL_Renderer &renderer, TTF_TextEngine *engine, TTF_Font *font) noexcept;
            TTF_Text *line(std::string_view text);
            void      clearCache();

            static constexpr std::size_t cache_limit = 1024;

            SDL_Renderer                                  *renderer;
            TTF_TextEngine                                *engine;
            TTF_Font                                      *font;
            float                                          density = 1.0F;
            std::unordered_map<std::string, TTF_Text *>    cache;
            mutable std::unordered_map<std::string, float> widths;
    };
}

#endif // PLAYER_UI_SDL_UI_SDL_CANVAS_HPP
