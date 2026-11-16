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
#ifndef PLAYER_UI_SDL_MEDIA_HPP
#define PLAYER_UI_SDL_MEDIA_HPP

#include "smil/document.hpp"

#include <SDL3/SDL_render.h>

#include <functional>
#include <memory>
#include <string>

namespace player
{
    /// What a medium reports that only the frontend knows: the end of a video, a file that cannot be played.
    enum class Outcome
    {
        Finished,
        Failed
    };

    /// Called by a medium; the window hands it to the core after the current event (never inside start()).
    using Report = std::function<void(Outcome outcome, const std::string &message)>;

    /// What media need from the window: the renderer, and a way to wake the main loop to draw a new frame.
    struct MediaContext
    {
        SDL_Renderer         *renderer = nullptr;
        std::function<void()> redraw;
    };

    /**
     * One medium in one region, from StartMedia to StopMedia (like the Media of the qwidget player). The timing is the
     * business of the core: a medium only shows itself and reports what only the frontend knows.
     *
     * smil and region point into the document of the current NewLayout, which the window keeps.
     */
    class Media
    {
        public:
            Media(const garlic::core::smil::Media &smil, const garlic::core::smil::Region &region, std::string source,
                  Report report);
            virtual ~Media() = default;

            Media(const Media &)            = delete;
            Media &operator=(const Media &) = delete;

            /// Shown in the region; false for audio and commands
            [[nodiscard]] virtual bool visible() const { return true; }

            virtual void start() = 0;
            virtual void stop() {}
            virtual void pause() {}
            virtual void resume() {}
            virtual void repeat() {} // RepeatMedia: video and audio again from the beginning, an image just stays

            /// Once per loop turn, e.g. to take the events of a player library
            virtual void update() {}

            /// Draws into area (pixels of the renderer); the clip of the region is set
            virtual void draw(SDL_Renderer &renderer, const SDL_FRect &area) = 0;

            /// For the debug dialog: title, else src of the SMIL (garlic-lib)
            [[nodiscard]] std::string label() const { return smil.title.empty() ? smil.src : smil.title; }

        protected:
            const garlic::core::smil::Media  &smil;
            const garlic::core::smil::Region &region;
            std::string                       source; // local path or URL, widgets with their query
            Report                            report;

            [[nodiscard]] std::string fit() const;        // of the medium, else of the region; lower case
            [[nodiscard]] std::string mediaAlign() const; // of the medium, else of the region; lower case
            [[nodiscard]] double      volume() const;     // soundLevel of the medium, else of the region: "50%" → 0.5
    };

    /// The class for media.kind; nullptr for kinds that are not shown (prefetch).
    std::unique_ptr<Media> createMedia(const garlic::core::smil::Media &smil, const garlic::core::smil::Region &region,
                                       const std::string &source, Report report, const MediaContext &context);

    /**
     * Where content of width × height goes in area for fit and mediaAlign, like the Image of the qwidget player:
     * fill stretches, meet keeps the aspect inside, meetBest like meet but never larger than the original, slice
     * covers the area (cut by the clip), anything else keeps the original size. The result is placed by mediaAlign
     * (topLeft if unknown, garlic-lib); a larger result is cut on the sides the alignment points away from.
     */
    SDL_FRect place(const SDL_FRect &area, float width, float height, const std::string &fit,
                    const std::string &media_align);
}

#endif // PLAYER_UI_SDL_MEDIA_HPP
