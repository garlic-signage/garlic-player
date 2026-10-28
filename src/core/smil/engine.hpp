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
#ifndef GARLIC_CORE_SMIL_ENGINE_HPP
#define GARLIC_CORE_SMIL_ENGINE_HPP

#include "common/clock.hpp"
#include "logger/logger.hpp"
#include "smil/document.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <queue>
#include <string>
#include <variant>
#include <vector>

namespace garlic::core::smil
{
    /// One start of a medium. Played three times means three instances.
    using InstanceId = std::uint32_t;

    /// Show a medium. The player reads fit, soundLevel, kind … from media.
    struct StartMedia
    {
        InstanceId               id = 0;
        const Media             *media = nullptr; // points into the Document
        std::string              source;          // local path or URL, widgets with their query
        std::vector<std::size_t> regions;         // indexes into Head::regions, all regions with this name
    };

    struct StopMedia
    {
        InstanceId id = 0;
    };

    /// repeatCount: play the instance again from the beginning (video, audio); an image just stays.
    struct RepeatMedia
    {
        InstanceId id = 0;
    };

    struct PauseMedia
    {
        InstanceId id = 0;
    };

    struct ResumeMedia
    {
        InstanceId id = 0;
    };

    using Command = std::variant<StartMedia, StopMedia, RepeatMedia, PauseMedia, ResumeMedia>;

    /// Access to the media cache, garlic-lib: MediaManager.
    class MediaSource
    {
        public:
            virtual ~MediaSource() = default;

            /// Local path, or the URL of an uncachable medium; nullopt = not available, the element is skipped.
            virtual std::optional<std::string> resolve(const Media &media) = 0;

            /// <prefetch>: only download, nothing is shown.
            virtual void prefetch(const Media &media) = 0;
    };

    struct EngineConfig
    {
        std::string player_id;   // smil-playerId() in expr
        std::string player_name; // smil-playerName() in expr

        /// A repeating container whose run played nothing (e.g. all media still downloading) is not repeated at
        /// once, but after this time. Otherwise it would loop without end in one poll().
        std::chrono::milliseconds empty_run_retry{1000};
    };

    /**
     * Plays a SMIL Document: events in, commands out. No timers and no threads: the host calls poll() and collects
     * the commands. All callbacks of the player (mediaFinished …) only change the state; their commands come with the
     * next poll(). The Document must live as long as the engine.
     */
    class Engine
    {
        public:
            Engine(const Document &document, const common::Clock &clock, MediaSource &media, logger::Logger &logger,
                   EngineConfig config = {});

            void start(); ///< the body begins
            void stop();  ///< stops everything, e.g. for a new index

            /// Works off everything that is due and returns the commands for the player.
            std::vector<Command> poll();

            /// When poll() is needed next; nullopt = only after an event of the player.
            std::optional<std::chrono::steady_clock::time_point> nextDeadline() const;

            /// The medium ended by itself (video, audio). Ignored if a dur decides the end.
            void mediaFinished(InstanceId id);

            /// The medium could not be played. Like an early end: a dur still decides the end, no play log.
            void mediaFailed(InstanceId id);

        private:
            enum class State
            {
                Idle,
                Active,
                Ended
            };

            /// Runtime state of one node of the tree; the tree itself stays unchanged.
            struct Runtime
            {
                const Media             *media     = nullptr;
                const Container         *container = nullptr;
                std::size_t              parent    = 0; // the body is its own parent
                std::vector<std::size_t> children;

                State         state      = State::Idle;
                std::uint32_t generation = 0; // changes when the node ends, so old scheduled events are ignored
                int           remaining_repeats = 1;

                // container
                std::size_t                           next_child = 0;
                std::chrono::steady_clock::time_point run_start;

                // media
                InstanceId                            instance = 0;
                bool                                  failed   = false;
                std::chrono::system_clock::time_point started_at; // for the play log
            };

            enum class EventType
            {
                SimpleEnd, // dur has elapsed
                Run        // run a container again (empty_run_retry)
            };

            struct Scheduled
            {
                std::chrono::steady_clock::time_point time;
                std::uint64_t                         order; // same time: first come, first served
                std::size_t                           node;
                std::uint32_t                         generation;
                EventType                             type;

                bool operator>(const Scheduled &other) const
                {
                    return time != other.time ? time > other.time : order > other.order;
                }
            };

            const Document        &document;
            const common::Clock   &clock;
            MediaSource           &media_source;
            logger::Logger        &logger;
            EngineConfig           config;

            std::vector<Runtime>   nodes; // [0] is the body
            std::priority_queue<Scheduled, std::vector<Scheduled>, std::greater<>> schedule;
            std::uint64_t          next_order    = 0;
            InstanceId             next_instance = 0;
            std::vector<Command>   pending;

            void        addNode(const Node &node, std::size_t parent);
            void        at(std::chrono::steady_clock::time_point time, std::size_t node, EventType type);
            std::optional<std::size_t> findInstance(InstanceId id) const;
            std::vector<std::size_t>   regionsOf(const Media &media) const;

            bool activate(std::size_t node);
            void beginRun(std::size_t container);
            void playNext(std::size_t container);
            void childEnded(std::size_t container);
            void containerSimpleEnd(std::size_t container);

            bool startMedia(std::size_t node);
            void mediaSimpleEnd(std::size_t node);
            void endMedia(std::size_t node);
            void earlyEnd(std::size_t node);
    };
}

#endif // GARLIC_CORE_SMIL_ENGINE_HPP
