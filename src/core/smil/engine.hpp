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
#include "smil/expr.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <random>
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

        /// Start value of the random generator for <seq> with pickNumber (shuffled once per engine). The host passes a
        /// random value (e.g. std::random_device); tests a fixed one, so the order is the same on every platform.
        std::uint32_t random_seed = 0;

        /// A skipped element (missing medium, no dur, prefetch …) ends after this time, garlic-lib: skipElement.
        /// It counts as ended like any other child (e.g. for endsync="first"), and a repeating container of skipped
        /// elements takes time instead of looping without end in one poll().
        std::chrono::milliseconds skip_delay{100};
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
                Waiting, // activated by the parent, waits for a begin time (or a trigger)
                Active,  // plays (or is skipped for skip_delay)
                Ended    // no further begin in this activation
            };

            /// An excl child that waits to play: paused (resumed with its rest) or deferred (started anew).
            struct Queued
            {
                std::size_t node;
                std::size_t priority_class; // index in the excl, 0 = highest
                bool        paused;
            };

            /// Runtime state of one node of the tree; the tree itself stays unchanged.
            struct Runtime
            {
                const Media             *media     = nullptr;
                const Container         *container = nullptr;
                std::size_t              parent    = 0; // the body is its own parent
                std::vector<std::size_t> children;

                State         state      = State::Idle;
                std::uint32_t generation = 0; // per interval: old dur/skip events are ignored
                std::uint32_t activation = 0; // per activation by the parent: old begin/end events are ignored
                int           remaining_repeats = 1;
                int           pending_begins    = 0;     // begin times of this activation still to come
                bool          open_begin        = false; // begin="indefinite": waits for a trigger
                bool          started_once      = false; // for restart="never"

                // container
                std::vector<std::size_t>              run_order;        // seq: the children of this run
                std::size_t                           next_child = 0;   // seq: index into run_order
                std::vector<std::size_t>              pick_order;       // seq with pickNumber: shuffled once
                std::size_t                           pick_next = 0;    // seq with pickNumber: index into pick_order

                // media
                InstanceId                            instance = 0;
                bool                                  failed   = false;
                bool                                  skipping = false; // skipped, ends after skip_delay
                bool                                  deferred = false; // excl: waits in the queue to start
                bool                                  paused   = false; // excl: frozen in the queue

                // excl
                std::optional<std::size_t> playing;   // the one child that plays
                std::vector<Queued>        queue;     // paused or deferred children
                std::chrono::system_clock::time_point started_at; // for the play log
            };

            /// Same time: ends before begins, so an interval that ends when the next begins makes room for it.
            enum class EventType
            {
                SimpleEnd, // dur of a medium or a container has elapsed
                Skipped,   // skip_delay of a skipped element has elapsed
                End,       // a time of the end attribute
                Begin      // a time of the begin attribute
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
                    if (time != other.time)
                        return time > other.time;
                    if (type != other.type)
                        return type > other.type;
                    return order > other.order;
                }
            };

            /// Events of a paused subtree: dur keeps its remaining time, begin and end their fixed time.
            struct Frozen
            {
                std::chrono::steady_clock::duration remaining;
                Scheduled                           event;
            };

            const Document        &document;
            const common::Clock   &clock;
            MediaSource           &media_source;
            logger::Logger        &logger;
            EngineConfig           config;

            std::vector<Runtime>   nodes; // [0] is the body
            std::vector<Scheduled> schedule; // min-heap (std::push_heap with std::greater): the next event first
            std::vector<std::pair<std::size_t, std::vector<Frozen>>> frozen; // paused node → its events
            std::uint64_t          next_order    = 0;
            InstanceId             next_instance = 0;
            std::vector<Command>   pending;
            std::mt19937           random;
            bool                   interrupting = false; // excl stop: the end of the stopped element is no normal end

            void        addNode(const Node &node, std::size_t parent);
            void        at(std::chrono::steady_clock::time_point time, std::size_t node, EventType type);
            void        push(Scheduled event);
            Scheduled   pop();
            std::optional<std::size_t> findInstance(InstanceId id) const;
            const std::string         &idOf(std::size_t node) const;
            std::vector<std::size_t>   regionsOf(const Media &media) const;

            bool hasDur(const Timing &timing) const; // dur with a time or indefinite: the dur decides the end
            const Timing &timingOf(std::size_t node) const;

            // lifecycle of every element
            void activate(std::size_t node);
            void tryStart(std::size_t node, std::chrono::milliseconds elapsed);
            void skip(std::size_t node);
            void endEvent(std::size_t node);
            void intervalEnded(std::size_t node);
            void settle(std::size_t node);
            void stopNode(std::size_t node);
            bool mayRepeat(std::size_t node) const;

            // containers
            void startContainer(std::size_t container, std::chrono::milliseconds elapsed);
            void beginRun(std::size_t container, std::chrono::milliseconds elapsed);
            std::vector<std::size_t> pickChildren(std::size_t container);
            void playNext(std::size_t container);
            void childEnded(std::size_t container, std::size_t child);
            void childrenDone(std::size_t container);
            void containerSimpleEnd(std::size_t container);
            void stopChildren(std::size_t container);

            // excl
            void startPriorityClass(std::size_t priority_class);
            bool admit(std::size_t node);
            void playFromQueue(std::size_t excl);
            void pauseNode(std::size_t node);
            void resumeNode(std::size_t node);
            void subtree(std::size_t node, std::vector<std::size_t> &nodes_out) const;
            std::size_t classIndex(std::size_t priority_class) const;

            // media
            void startMedia(std::size_t node, std::chrono::milliseconds elapsed);
            void mediaSimpleEnd(std::size_t node);
            void endMedia(std::size_t node);
            void earlyEnd(std::size_t node);
    };
}

#endif // GARLIC_CORE_SMIL_ENGINE_HPP
