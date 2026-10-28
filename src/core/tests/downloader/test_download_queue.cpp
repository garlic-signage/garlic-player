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
#include <doctest/doctest.h>

#include "common/memory_file_system.hpp"
#include "downloader/download_queue.hpp"
#include "fake_clock.hpp"
#include "fake_http_transport.hpp"
#include "memory_inventory.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace garlic::core::downloader;
using garlic::core::common::MemoryFileSystem;
using garlic::core::downloader::tests::FakeHttpTransport;
using garlic::core::downloader::tests::MemoryInventory;
using garlic::core::tests::FakeClock;

namespace
{
    std::string mediaUrl(int i)
    {
        return "http://example.com/media/" + std::to_string(i) + ".jpg";
    }

    struct Fixture
    {
        FakeClock                   clock;
        MemoryFileSystem            fs{clock};
        MemoryInventory             inventory;
        FakeHttpTransport           transport;
        std::vector<DownloadResult> results;

        DownloadQueue queue;

        explicit Fixture(std::size_t max_parallel = 2)
            : queue(transport, fs, inventory, clock, config(), max_parallel,
                    [this](const DownloadResult &r) { results.push_back(r); })
        {
        }

        static DownloaderConfig config()
        {
            DownloaderConfig c;
            c.cache_dir = "cache";
            return c;
        }

        /// Registers a downloadable image. A chunk size of 1 makes the download take several polls.
        void serve(const std::string &url, std::string body = "data")
        {
            FakeHttpTransport::Response r;
            r.headers.add("Content-Type", "image/jpeg");
            r.headers.add("Content-Length", std::to_string(body.size()));
            r.headers.add("ETag", "\"v1\"");
            r.body       = std::move(body);
            r.chunk_size = 1;
            transport.respond(HttpMethod::Head, url, r);
            transport.respond(HttpMethod::Get, url, r);
        }

        /// Runs until the queue is idle. Returns the highest number of parallel requests seen.
        std::size_t runUntilIdle()
        {
            std::size_t max_active = 0;
            for (int i = 0; i < 1000 && !queue.isIdle(); ++i)
            {
                max_active = std::max(max_active, transport.activeCount());
                transport.poll();
                queue.poll();
            }
            return max_active;
        }

        std::vector<std::string> finishedUrls() const
        {
            std::vector<std::string> urls;
            for (const auto &r : results)
                urls.push_back(r.url);
            return urls;
        }
    };
}

TEST_CASE("all jobs finish exactly once, in the order they were added")
{
    Fixture f(1);
    for (int i = 0; i < 3; ++i)
    {
        f.serve(mediaUrl(i));
        f.queue.add(mediaUrl(i));
    }

    f.runUntilIdle();

    CHECK(f.finishedUrls() == std::vector<std::string>{mediaUrl(0), mediaUrl(1), mediaUrl(2)});
    CHECK(std::ranges::all_of(f.results, [](const auto &r) { return r.status == DownloadStatus::Downloaded; }));
}

TEST_CASE("no more than max_parallel downloads run at the same time")
{
    for (std::size_t max_parallel : {1u, 2u, 4u})
    {
        CAPTURE(max_parallel);
        Fixture f(max_parallel);
        for (int i = 0; i < 6; ++i)
        {
            f.serve(mediaUrl(i));
            f.queue.add(mediaUrl(i));
        }

        CHECK(f.queue.runningCount() == max_parallel);
        CHECK(f.queue.pendingCount() == 6 - max_parallel);

        CHECK(f.runUntilIdle() == max_parallel);
        CHECK(f.results.size() == 6);
    }
}

TEST_CASE("max_parallel 0 is treated as 1")
{
    Fixture f(0);
    f.serve(mediaUrl(0));
    f.queue.add(mediaUrl(0));

    CHECK(f.queue.runningCount() == 1);
    f.runUntilIdle();
    CHECK(f.results.size() == 1);
}

TEST_CASE("a URL that is waiting or running is not added twice")
{
    Fixture f(1);
    f.serve(mediaUrl(0));
    f.serve(mediaUrl(1));

    f.queue.add(mediaUrl(0));
    f.queue.add(mediaUrl(1));
    f.queue.add(mediaUrl(0)); // running
    f.queue.add(mediaUrl(1)); // waiting

    CHECK(f.queue.pendingCount() == 1);
    f.runUntilIdle();
    CHECK(f.finishedUrls() == std::vector<std::string>{mediaUrl(0), mediaUrl(1)});
}

TEST_CASE("a finished URL can be added again")
{
    Fixture f;
    f.serve(mediaUrl(0));

    f.queue.add(mediaUrl(0));
    f.runUntilIdle();
    f.queue.add(mediaUrl(0));
    f.runUntilIdle();

    REQUIRE(f.results.size() == 2);
    CHECK(f.results[1].status == DownloadStatus::NotModified);
}

TEST_CASE("a failed job does not stop the others")
{
    Fixture f(1);
    f.serve(mediaUrl(0));
    f.serve(mediaUrl(2));

    f.queue.add(mediaUrl(0));
    f.queue.add(mediaUrl(1)); // not registered: 404
    f.queue.add(mediaUrl(2));
    f.runUntilIdle();

    REQUIRE(f.results.size() == 3);
    CHECK(f.results[0].status == DownloadStatus::Downloaded);
    CHECK(f.results[1].status == DownloadStatus::Failed);
    CHECK(f.results[2].status == DownloadStatus::Downloaded);
}

TEST_CASE("clear() cancels everything without callbacks")
{
    Fixture f(2);
    for (int i = 0; i < 5; ++i)
    {
        f.serve(mediaUrl(i));
        f.queue.add(mediaUrl(i));
    }
    f.transport.poll();

    f.queue.clear();
    f.runUntilIdle();
    for (int i = 0; i < 20; ++i)
        f.transport.poll();

    CHECK(f.results.empty());
    CHECK(f.queue.isIdle());
    CHECK(f.transport.activeCount() == 0);
}

TEST_CASE("jobs can be added from inside the callback")
{
    FakeClock                clock;
    MemoryFileSystem         fs{clock};
    MemoryInventory          inventory;
    FakeHttpTransport        transport;
    std::vector<std::string> finished;

    for (int i = 0; i < 2; ++i)
    {
        FakeHttpTransport::Response r;
        r.headers.add("Content-Type", "image/jpeg");
        r.body = "data";
        transport.respond(HttpMethod::Head, mediaUrl(i), r);
        transport.respond(HttpMethod::Get, mediaUrl(i), r);
    }

    std::unique_ptr<DownloadQueue> queue;
    queue = std::make_unique<DownloadQueue>(transport, fs, inventory, clock, Fixture::config(), 1,
                                            [&](const DownloadResult &r)
                                            {
                                                finished.push_back(r.url);
                                                if (r.url == mediaUrl(0)) // e.g. the index lists new media
                                                    queue->add(mediaUrl(1));
                                            });

    queue->add(mediaUrl(0));
    for (int i = 0; i < 100 && !queue->isIdle(); ++i)
        transport.poll();

    CHECK(finished == std::vector<std::string>{mediaUrl(0), mediaUrl(1)});
}
