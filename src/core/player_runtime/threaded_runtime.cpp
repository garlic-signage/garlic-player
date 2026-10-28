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
#include "player_runtime/threaded_runtime.hpp"

#include <algorithm>
#include <iterator>

namespace garlic::core
{
    ThreadedRuntime::ThreadedRuntime(common::FileSystem &f, const common::Clock &c, downloader::HttpTransport &t,
                                     PlayerRuntimeConfig cfg, Wake w)
        : fs(f), clock(c), transport(t), config(std::move(cfg)), wake(std::move(w))
    {
    }

    ThreadedRuntime::~ThreadedRuntime()
    {
        stop();
    }

    std::expected<void, std::string> ThreadedRuntime::start()
    {
        if (thread.joinable())
            return std::unexpected("already started");

        std::expected<void, std::string> started;
        bool                             ready = false;
        stopping                               = false;
        thread = std::thread([this, &started, &ready] { run(&started, &ready); });

        std::unique_lock lock(mutex);
        changed.wait(lock, [&] { return ready; });
        lock.unlock();
        if (!started)
            thread.join();
        return started;
    }

    void ThreadedRuntime::post(Call call)
    {
        {
            std::lock_guard lock(mutex);
            calls.push_back(std::move(call));
        }
        changed.notify_all();
    }

    std::vector<CoreEvent> ThreadedRuntime::takeEvents()
    {
        std::lock_guard lock(mutex);
        return std::exchange(events, {});
    }

    void ThreadedRuntime::stop()
    {
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        changed.notify_all();
        if (thread.joinable())
            thread.join();
    }

    /// The core thread: the runtime lives only here.
    void ThreadedRuntime::run(std::expected<void, std::string> *started, bool *ready)
    {
        PlayerRuntimeApi runtime(fs, clock, transport, config);
        auto             result = runtime.start();
        {
            std::lock_guard lock(mutex);
            *started = result;
            *ready   = true;
        }
        changed.notify_all(); // started and ready belong to start() and are not touched any more
        if (!result)
            return;

        std::unique_lock lock(mutex);
        while (!stopping)
        {
            std::vector<Call> due = std::exchange(calls, {});
            lock.unlock();
            for (Call &call : due)
                call(runtime);
            std::vector<CoreEvent> fresh    = runtime.poll();
            auto                   deadline = runtime.nextDeadline();
            if (!fresh.empty())
            {
                {
                    std::lock_guard append(mutex);
                    std::ranges::move(fresh, std::back_inserter(events));
                }
                if (wake)
                    wake();
            }
            lock.lock();

            auto woken = [this] { return stopping || !calls.empty(); };
            if (!deadline)
                changed.wait(lock, woken);
            else // the time of the runtime's clock, waited in real time
                changed.wait_for(lock, std::max(*deadline - clock.monotonic(), std::chrono::steady_clock::duration{}), woken);
        }
    }
}
