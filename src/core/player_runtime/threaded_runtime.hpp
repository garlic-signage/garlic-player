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
#ifndef GARLIC_CORE_PLAYER_RUNTIME_THREADED_RUNTIME_HPP
#define GARLIC_CORE_PLAYER_RUNTIME_THREADED_RUNTIME_HPP

#include "player_runtime/player_runtime_api.hpp"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace garlic::core
{
    /**
     * Runs a PlayerRuntimeApi in a thread of its own, for native frontends whose UI thread must not wait for the core.
     * The runtime is created, started, polled and destroyed only in that thread, so curl, libmicrohttpd and SQLite see
     * one thread like without the helper.
     *
     *   ThreadedRuntime runtime(fs, clock, transport, config, [&] { wake the UI thread, e.g. a queued signal });
     *   runtime.start();
     *   runtime.post([id](PlayerRuntimeApi &r) { r.mediaFinished(id); });   // from any thread
     *   UI thread, after the wake: for (const CoreEvent &event : runtime.takeEvents()) …
     *
     * The thread waits until the next deadline of the runtime or until post()/stop(). fs, clock and transport are only
     * used by the core thread after start(). Everything else of the runtime (configuration() …) is reached by post().
     */
    class ThreadedRuntime
    {
        public:
            using Call = std::function<void(PlayerRuntimeApi &)>;
            using Wake = std::function<void()>; // called in the core thread when new events are there

            ThreadedRuntime(common::FileSystem &fs, const common::Clock &clock, downloader::HttpTransport &transport,
                            PlayerRuntimeConfig config, Wake wake);
            ~ThreadedRuntime();

            ThreadedRuntime(const ThreadedRuntime &)            = delete;
            ThreadedRuntime &operator=(const ThreadedRuntime &) = delete;

            /// Starts the thread and waits until PlayerRuntimeApi::start() has run there; its error ends the thread.
            std::expected<void, std::string> start();

            /// Runs call in the core thread before the next poll().
            void post(Call call);

            /// The events since the last call, in their order.
            std::vector<CoreEvent> takeEvents();

            /// Ends the thread; the runtime is destroyed there. Events not taken stay available.
            void stop();

        private:
            common::FileSystem        &fs;
            const common::Clock       &clock;
            downloader::HttpTransport &transport;
            PlayerRuntimeConfig        config;
            Wake                       wake;

            std::mutex              mutex;
            std::condition_variable changed;
            std::vector<Call>       calls;
            std::vector<CoreEvent>  events;
            bool                    stopping = false;
            std::thread             thread;

            void run(std::expected<void, std::string> *started, bool *ready);
    };
}

#endif // GARLIC_CORE_PLAYER_RUNTIME_THREADED_RUNTIME_HPP
