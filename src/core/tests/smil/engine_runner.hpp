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
#ifndef GARLIC_CORE_TESTS_SMIL_ENGINE_RUNNER_HPP
#define GARLIC_CORE_TESTS_SMIL_ENGINE_RUNNER_HPP

#include "common/memory_file_system.hpp"
#include "fake_clock.hpp"
#include "helpers.hpp"
#include "logger/logger.hpp"
#include "smil/engine.hpp"

#include <format>
#include <map>
#include <set>

namespace garlic::core::tests
{
    /// MediaSource of the tests: every src is in the cache as "cache/<src>", except the ones in missing.
    class FakeMediaSource final : public smil::MediaSource
    {
        public:
            std::set<std::string>    missing;
            std::vector<std::string> prefetched;

            std::optional<std::string> resolve(const smil::Media &media) override
            {
                if (missing.contains(media.src))
                    return std::nullopt;
                return "cache/" + media.src;
            }

            void prefetch(const smil::Media &media) override { prefetched.push_back(media.src); }
    };

    /**
     * Runs an engine on a FakeClock and writes the commands as timeline:
     *   "0s start a | 2s stop a | 2s start b"
     * Videos and audios end after the length in media_length (by src), like a player would report it.
     */
    class EngineRunner
    {
        public:
            FakeClock                                        clock;
            common::MemoryFileSystem                         fs{clock};
            logger::Logger                                   logger;
            FakeMediaSource                                  media;
            smil::Document                                   document;
            std::map<std::string, std::chrono::milliseconds> media_length; // by src
            std::set<std::string>                            failing;      // src the player cannot play
            std::vector<smil::StartMedia>                    starts;       // every StartMedia, for details
            std::unique_ptr<smil::Engine>                    engine;

            explicit EngineRunner(std::string_view smil) : logger(fs, clock, loggerConfig())
            {
                auto parsed = smil::parse(smil);
                REQUIRE(parsed.has_value());
                document = std::move(*parsed);
            }

            static EngineRunner body(std::string_view body, std::string_view head = "")
            {
                return EngineRunner("<smil><head>" + std::string(head) + "</head><body>" + std::string(body) +
                                    "</body></smil>");
            }

            /// Starts the engine and runs until the given time; returns the timeline.
            std::string run(std::chrono::milliseconds until)
            {
                origin = clock.monotonic();
                engine = std::make_unique<smil::Engine>(document, clock, media, logger);
                engine->start();
                return runFor(until);
            }

            /// Runs on from the current time until the given time since the start.
            std::string runFor(std::chrono::milliseconds until)
            {
                auto end = origin + until;
                while (true)
                {
                    collect();
                    auto next = nextEvent();
                    if (!next || *next > end)
                        break;
                    clock.advance(*next - clock.monotonic());
                    for (auto it = finishes.begin(); it != finishes.end() && it->first <= clock.monotonic();)
                    {
                        if (failing.contains(sources[it->second]))
                            engine->mediaFailed(it->second);
                        else
                            engine->mediaFinished(it->second);
                        it = finishes.erase(it);
                    }
                }
                if (clock.monotonic() < end)
                    clock.advance(end - clock.monotonic());
                return timeline;
            }

            /// Calls poll() and records its commands.
            void collect()
            {
                for (const smil::Command &command : engine->poll())
                    record(command);
            }

            std::string playLog() const { return fs.content(logger.path(logger::LogFile::Play)).value_or(""); }

        private:
            std::chrono::steady_clock::time_point origin;
            std::string                           timeline;
            std::map<smil::InstanceId, std::string>                         names;
            std::map<smil::InstanceId, std::string>                         sources;
            std::multimap<std::chrono::steady_clock::time_point, smil::InstanceId> finishes;

            static logger::LoggerConfig loggerConfig()
            {
                logger::LoggerConfig config;
                config.log_dir = "logs";
                return config;
            }

            std::optional<std::chrono::steady_clock::time_point> nextEvent() const
            {
                auto next = engine->nextDeadline();
                if (!finishes.empty() && (!next || finishes.begin()->first < *next))
                    next = finishes.begin()->first;
                if (next && *next < clock.monotonic())
                    next = clock.monotonic();
                return next;
            }

            std::string now() const
            {
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(clock.monotonic() - origin).count();
                return ms % 1000 == 0 ? std::format("{}s", ms / 1000) : std::format("{}.{:03}s", ms / 1000, ms % 1000);
            }

            void add(const std::string &entry) { timeline += (timeline.empty() ? "" : " | ") + now() + " " + entry; }

            void scheduleFinish(smil::InstanceId id)
            {
                const std::string &src = sources[id];
                if (auto length = media_length.find(src); length != media_length.end())
                    finishes.emplace(clock.monotonic() + length->second, id);
                else if (failing.contains(src))
                    finishes.emplace(clock.monotonic(), id);
            }

            void record(const smil::Command &command)
            {
                if (const auto *start = std::get_if<smil::StartMedia>(&command))
                {
                    starts.push_back(*start);
                    names[start->id]   = start->media->id;
                    sources[start->id] = start->media->src;
                    add("start " + start->media->id);
                    scheduleFinish(start->id);
                }
                else if (const auto *stop = std::get_if<smil::StopMedia>(&command))
                {
                    add("stop " + names[stop->id]);
                    std::erase_if(finishes, [&](const auto &f) { return f.second == stop->id; });
                }
                else if (const auto *repeat = std::get_if<smil::RepeatMedia>(&command))
                {
                    add("repeat " + names[repeat->id]);
                    scheduleFinish(repeat->id);
                }
                else if (const auto *pause = std::get_if<smil::PauseMedia>(&command))
                    add("pause " + names[pause->id]);
                else if (const auto *resume = std::get_if<smil::ResumeMedia>(&command))
                    add("resume " + names[resume->id]);
            }
    };
}

#endif // GARLIC_CORE_TESTS_SMIL_ENGINE_RUNNER_HPP
