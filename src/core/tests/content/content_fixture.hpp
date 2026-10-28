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
#ifndef GARLIC_CORE_TESTS_CONTENT_FIXTURE_HPP
#define GARLIC_CORE_TESTS_CONTENT_FIXTURE_HPP

#include <doctest/doctest.h>

#include "common/memory_file_system.hpp"
#include "downloader/download_queue.hpp"
#include "fake_clock.hpp"
#include "fake_http_transport.hpp"
#include "logger/logger.hpp"
#include "memory_inventory.hpp"

#include <miniz.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace garlic::core::tests
{
    /// Downloads without network: a FakeHttpTransport serves the files, results go to on_result.
    struct ContentFixture
    {
        FakeClock                                      clock;
        common::MemoryFileSystem                       fs{clock};
        downloader::tests::MemoryInventory             inventory;
        downloader::tests::FakeHttpTransport           transport;
        logger::Logger                                 logger{fs, clock, loggerConfig()};
        std::function<void(const downloader::DownloadResult &)> on_result;
        downloader::DownloadQueue queue{transport, fs, inventory, clock, downloaderConfig(), 2,
                                        [this](const downloader::DownloadResult &r)
                                        {
                                            if (on_result)
                                                on_result(r);
                                        }};

        static downloader::DownloaderConfig downloaderConfig()
        {
            downloader::DownloaderConfig config;
            config.cache_dir = "cache";
            return config;
        }

        static logger::LoggerConfig loggerConfig()
        {
            logger::LoggerConfig config;
            config.log_dir = "logs";
            return config;
        }

        /// Serves a file; a new etag makes the next request a new version.
        void serve(const std::string &url, std::string body, const std::string &type, const std::string &etag = "v1")
        {
            downloader::tests::FakeHttpTransport::Response r;
            r.headers.add("Content-Type", type);
            r.headers.add("Content-Length", std::to_string(body.size()));
            r.headers.add("ETag", "\"" + etag + "\"");
            r.body       = std::move(body);
            r.chunk_size = 1 << 20;
            transport.respond(downloader::HttpMethod::Head, url, r);
            transport.respond(downloader::HttpMethod::Get, url, r);
        }

        void runUntilIdle()
        {
            for (int i = 0; i < 1000 && !queue.isIdle(); ++i)
            {
                transport.poll();
                queue.poll();
            }
        }

        std::string events() const { return fs.content("logs/event_log.xml").value_or(""); }

        std::size_t requests(const std::string &url) const
        {
            std::size_t n = 0;
            for (const auto &request : transport.requests)
                n += request.url == url;
            return n;
        }
    };

    /// A zip archive in memory: name → content.
    inline std::string makeZip(const std::map<std::string, std::string> &files)
    {
        mz_zip_archive zip{};
        REQUIRE(mz_zip_writer_init_heap(&zip, 0, 0));
        for (const auto &[name, content] : files)
            REQUIRE(mz_zip_writer_add_mem(&zip, name.c_str(), content.data(), content.size(), MZ_DEFAULT_COMPRESSION));
        void       *data = nullptr;
        std::size_t size = 0;
        REQUIRE(mz_zip_writer_finalize_heap_archive(&zip, &data, &size));
        std::string result(static_cast<const char *>(data), size);
        mz_zip_writer_end(&zip);
        return result;
    }

    inline void writeFile(common::FileSystem &fs, const std::string &path, std::string_view content)
    {
        auto file = fs.openForWrite(path, common::WriteMode::Truncate);
        REQUIRE(file.has_value());
        REQUIRE((*file)->write(std::as_bytes(std::span(content.data(), content.size()))).has_value());
        REQUIRE((*file)->close().has_value());
    }
}

#endif // GARLIC_CORE_TESTS_CONTENT_FIXTURE_HPP
