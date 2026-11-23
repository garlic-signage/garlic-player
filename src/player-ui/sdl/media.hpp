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

#include "place.hpp"
#include "smil/document.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_render.h>

#include <functional>
#include <memory>
#include <string>

namespace player
{
    /// What a medium reports that only the frontend knows: the end of a video, a file that cannot be played and why
    /// (smil::MediaError of the core, the events MEDIA_NOT_AVAILABLE, MEDIA_OPEN_ERROR, MEDIA_PLAYBACK_ERROR).
    enum class Outcome
    {
        Finished,
        NotAvailable, // the file is not there
        OpenFailed,   // it does not start
        PlaybackFailed // it stops while playing
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
     * One medium in one region, from StartMedia to StopMedia. The timing is the
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

            /// Takes mouse and keyboard itself (web pages): the window hands them over instead of activating the
            /// element or sending access keys, like the Qt players where the web view takes clicks and focus
            [[nodiscard]] virtual bool interactive() const { return false; }

            /// An SDL mouse, wheel, key or text event; x and y of mouse events are pixels relative to the region
            virtual void input(const SDL_Event & /*event*/, float /*x*/, float /*y*/) {}

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
}

#endif // PLAYER_UI_SDL_MEDIA_HPP
