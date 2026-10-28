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

#include "common/md5.hpp"
#include "common/memory_file_system.hpp"
#include "downloader/downloader.hpp"
#include "fake_clock.hpp"
#include "fake_http_transport.hpp"
#include "memory_inventory.hpp"

#include <optional>
#include <string>

using namespace garlic::core::downloader;
using garlic::core::common::Md5;
using garlic::core::common::MemoryFileSystem;
using garlic::core::common::WriteMode;
using garlic::core::downloader::tests::FakeHttpTransport;
using garlic::core::downloader::tests::MemoryInventory;
using garlic::core::tests::FakeClock;
using namespace std::chrono_literals;

namespace
{
    const std::string media_url = "http://example.com/media/a.jpg";
    const std::string cache_file = "cache/" + Md5::hex(media_url) + ".jpg";

    FakeHttpTransport::Response response(int status, std::string type, std::string body, std::string etag = {})
    {
        FakeHttpTransport::Response r;
        r.status = status;
        r.body   = std::move(body);
        if (!type.empty())
            r.headers.add("Content-Type", type);
        r.headers.add("Content-Length", std::to_string(r.body.size()));
        if (!etag.empty())
            r.headers.add("ETag", etag);
        return r;
    }

    DownloaderConfig config()
    {
        DownloaderConfig c;
        c.cache_dir  = "cache";
        c.user_agent = "test";
        return c;
    }

    struct Fixture
    {
        FakeClock         clock;
        MemoryFileSystem  fs{clock};
        MemoryInventory   inventory;
        FakeHttpTransport transport;
        Downloader        downloader{transport, fs, inventory, clock, config()};

        std::optional<DownloadResult> result;

        /// Starts a job and drives transport and downloader until it finished.
        DownloadResult download(const std::string &url, const std::string &file_name = {})
        {
            result.reset();
            REQUIRE(downloader.start(url, file_name, [this](const DownloadResult &r) { result = r; }));
            for (int i = 0; i < 1000 && downloader.isBusy(); ++i)
            {
                transport.poll();
                downloader.poll();
            }
            REQUIRE(result.has_value());
            return *result;
        }

        void putFile(const std::string &path, std::string_view content)
        {
            auto file = fs.openForWrite(path, WriteMode::Truncate);
            REQUIRE(file.has_value());
            REQUIRE((*file)->write(std::as_bytes(std::span(content.data(), content.size()))).has_value());
            REQUIRE((*file)->close().has_value());
        }

        /// A completely downloaded file from an earlier run.
        void putCached(std::string_view content, const std::string &etag)
        {
            putFile(cache_file, content);
            inventory.save({media_url, Md5::hex(media_url) + ".jpg", etag, "", "image/jpeg", content.size(),
                            InventoryState::Complete});
        }

        std::string content(const std::string &path) const { return fs.content(path).value_or("<missing>"); }

        std::optional<std::string_view> requestHeader(std::size_t index, std::string_view name) const
        {
            return transport.requests.at(index).headers.find(name);
        }
    };
}

// ------------------------------------------------------------------ download

TEST_CASE("first download: HEAD, GET, file in the cache, inventory complete")
{
    Fixture f;
    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "data", "\"v1\""));
    f.transport.respond(HttpMethod::Get, media_url, response(200, "image/jpeg", "data", "\"v1\""));

    auto r = f.download(media_url);

    CHECK(r.status == DownloadStatus::Downloaded);
    CHECK(r.local_path == cache_file);
    CHECK(f.content(cache_file) == "data");
    CHECK_FALSE(f.fs.exists(cache_file + ".part"));

    REQUIRE(f.transport.requests.size() == 2);
    CHECK(f.transport.requests[0].method == HttpMethod::Head);
    CHECK(f.transport.requests[1].method == HttpMethod::Get);
    CHECK(f.requestHeader(0, "User-Agent") == "test");

    auto entry = f.inventory.find(media_url);
    REQUIRE(entry.has_value());
    CHECK(entry->state == InventoryState::Complete);
    CHECK(entry->etag == "\"v1\"");
    CHECK(entry->content_length == 4);
}

TEST_CASE("changed file: the playing file stays, the new one waits as .ready")
{
    Fixture f;
    f.putCached("old", "\"v1\"");
    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "new!", "\"v2\""));
    f.transport.respond(HttpMethod::Get, media_url, response(200, "image/jpeg", "new!", "\"v2\""));

    auto r = f.download(media_url);

    CHECK(r.status == DownloadStatus::Downloaded);
    CHECK(r.local_path == cache_file);
    CHECK(f.content(cache_file) == "old");
    CHECK(f.content(cache_file + ".ready") == "new!");
    CHECK(f.inventory.find(media_url)->etag == "\"v2\"");
}

TEST_CASE("a fixed file name is used as given")
{
    Fixture f;
    f.transport.respond(HttpMethod::Head, "http://example.com/index.php?player=1",
                        response(200, "application/smil+xml", "<smil/>"));
    f.transport.respond(HttpMethod::Get, "http://example.com/index.php?player=1",
                        response(200, "application/smil+xml", "<smil/>"));

    auto r = f.download("http://example.com/index.php?player=1", "index.smil");

    CHECK(r.status == DownloadStatus::Downloaded);
    CHECK(r.local_path == "cache/index.smil");
    CHECK(f.content("cache/index.smil") == "<smil/>");
}

// ------------------------------------------------------------------ file names

TEST_CASE("file name: extension from the URL")
{
    Fixture f;
    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "data"));
    f.transport.respond(HttpMethod::Get, media_url, response(200, "image/jpeg", "data"));

    CHECK(f.download(media_url).local_path == cache_file);
}

TEST_CASE("file name: extension from the content type if the URL has none or is a script")
{
    for (std::string url : {"http://example.com/media/12345", "http://example.com/get.php?id=7&ext=jpg"})
    {
        CAPTURE(url);
        Fixture f;
        f.transport.respond(HttpMethod::Head, url, response(200, "image/png; charset=binary", "data"));
        f.transport.respond(HttpMethod::Get, url, response(200, "image/png", "data"));

        CHECK(f.download(url).local_path == "cache/" + Md5::hex(url) + ".png");
    }
}

TEST_CASE("file name: URL extension wins over application/octet-stream")
{
    Fixture     f;
    std::string url = "http://example.com/video.mp4";
    f.transport.respond(HttpMethod::Head, url, response(200, "video/mp4", "data"));
    f.transport.respond(HttpMethod::Get, url, response(200, "video/mp4", "data"));

    CHECK(f.download(url).local_path == "cache/" + Md5::hex(url) + ".mp4");
}

// ------------------------------------------------------------------ not modified

TEST_CASE("cached file: conditional HEAD, 304 means not modified")
{
    Fixture f;
    f.putCached("old", "\"v1\"");
    f.transport.respond(HttpMethod::Head, media_url, response(304, "", ""));

    auto r = f.download(media_url);

    CHECK(r.status == DownloadStatus::NotModified);
    CHECK(r.local_path == cache_file);
    CHECK(f.requestHeader(0, "If-None-Match") == "\"v1\"");
    CHECK(f.transport.requests.size() == 1);
}

TEST_CASE("server ignores the condition but sends the same ETag: no GET")
{
    Fixture f;
    f.putCached("old", "\"v1\"");
    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "old", "\"v1\""));

    auto r = f.download(media_url);

    CHECK(r.status == DownloadStatus::NotModified);
    CHECK(f.transport.requests.size() == 1);
}

TEST_CASE("without ETag Last-Modified and size decide")
{
    Fixture f;
    f.putFile(cache_file, "old");
    f.inventory.save({media_url, Md5::hex(media_url) + ".jpg", "", "Wed, 01 Jan 2025 10:00:00 GMT", "image/jpeg", 3,
                      InventoryState::Complete});

    auto head = response(200, "image/jpeg", "old");
    head.headers.add("Last-Modified", "Wed, 01 Jan 2025 10:00:00 GMT");
    f.transport.respond(HttpMethod::Head, media_url, head);

    auto r = f.download(media_url);

    CHECK(r.status == DownloadStatus::NotModified);
    CHECK(f.requestHeader(0, "If-Modified-Since") == "Wed, 01 Jan 2025 10:00:00 GMT");
}

TEST_CASE("no conditional HEAD if the cached file was deleted")
{
    Fixture f;
    f.inventory.save({media_url, Md5::hex(media_url) + ".jpg", "\"v1\"", "", "image/jpeg", 4, InventoryState::Complete});
    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "data", "\"v1\""));
    f.transport.respond(HttpMethod::Get, media_url, response(200, "image/jpeg", "data", "\"v1\""));

    auto r = f.download(media_url);

    CHECK_FALSE(f.requestHeader(0, "If-None-Match").has_value());
    CHECK(r.status == DownloadStatus::Downloaded);
    CHECK(f.content(cache_file) == "data");
}

// ------------------------------------------------------------------ redirects

TEST_CASE("redirects are followed, the file name stays that of the original URL")
{
    Fixture     f;
    std::string cdn = "http://cdn.example.com/files/a.jpg?sig=123";

    auto first = response(302, "", "");
    first.headers.add("Location", "/moved/a.jpg");
    f.transport.respond(HttpMethod::Head, media_url, first);

    auto second = response(301, "", "");
    second.headers.add("Location", cdn);
    f.transport.respond(HttpMethod::Head, "http://example.com/moved/a.jpg", second);

    f.transport.respond(HttpMethod::Head, cdn, response(200, "image/jpeg", "data"));
    f.transport.respond(HttpMethod::Get, cdn, response(200, "image/jpeg", "data"));

    auto r = f.download(media_url);

    CHECK(r.status == DownloadStatus::Downloaded);
    CHECK(r.local_path == cache_file);
    REQUIRE(f.transport.requests.size() == 4);
    CHECK(f.transport.requests[3].url == cdn);
}

TEST_CASE("relative redirect without leading slash")
{
    Fixture f;
    auto    redirect = response(307, "", "");
    redirect.headers.add("Location", "b.jpg");
    f.transport.respond(HttpMethod::Head, media_url, redirect);
    f.transport.respond(HttpMethod::Head, "http://example.com/media/b.jpg", response(200, "image/jpeg", "data"));
    f.transport.respond(HttpMethod::Get, "http://example.com/media/b.jpg", response(200, "image/jpeg", "data"));

    CHECK(f.download(media_url).status == DownloadStatus::Downloaded);
}

TEST_CASE("too many redirects fail")
{
    Fixture f;
    auto    loop = response(302, "", "");
    loop.headers.add("Location", media_url);
    f.transport.respond(HttpMethod::Head, media_url, loop);

    auto r = f.download(media_url);

    CHECK(r.status == DownloadStatus::Failed);
    CHECK(r.error == "too many redirects");
    CHECK(f.transport.requests.size() == 6); // first request + 5 redirects
}

// ------------------------------------------------------------------ errors: the player keeps playing

TEST_CASE("HTTP error: the cached file stays and is returned")
{
    for (int status : {404, 500, 503})
    {
        CAPTURE(status);
        Fixture f;
        f.putCached("old", "\"v1\"");
        f.transport.respond(HttpMethod::Head, media_url, response(status, "text/plain", "error"));

        auto r = f.download(media_url);

        CHECK(r.status == DownloadStatus::Failed);
        CHECK(r.http_status == status);
        CHECK(r.local_path == cache_file);
        CHECK(f.content(cache_file) == "old");
    }
}

TEST_CASE("HTTP error without cached file: no local path")
{
    Fixture f;
    auto    r = f.download(media_url); // fake answers 404

    CHECK(r.status == DownloadStatus::Failed);
    CHECK(r.local_path.empty());
}

TEST_CASE("connection lost during GET: the cached file stays")
{
    Fixture f;
    f.putCached("old", "\"v1\"");
    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "new!", "\"v2\""));
    auto get  = response(200, "image/jpeg", "new!", "\"v2\"");
    get.error = TransportError{TransportErrorCode::ConnectionFailed, "connection reset"};
    f.transport.respond(HttpMethod::Get, media_url, get);

    auto r = f.download(media_url);

    CHECK(r.status == DownloadStatus::Failed);
    CHECK(r.error == "connection reset");
    CHECK(r.local_path == cache_file);
    CHECK(f.content(cache_file) == "old");
    CHECK_FALSE(f.fs.exists(cache_file + ".ready"));
}

TEST_CASE("GET answers with an error status: the cached file stays")
{
    Fixture f;
    f.putCached("old", "\"v1\"");
    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "new!", "\"v2\""));
    f.transport.respond(HttpMethod::Get, media_url, response(500, "text/plain", "server error"));

    auto r = f.download(media_url);

    CHECK(r.status == DownloadStatus::Failed);
    CHECK(r.error == "HTTP 500");
    CHECK(r.local_path == cache_file);
    CHECK(f.content(cache_file) == "old");
}

TEST_CASE("incomplete file: .part is kept for resuming, the cached file stays")
{
    Fixture f;
    f.putCached("old", "\"v1\"");
    auto head = response(200, "image/jpeg", "0123456789", "\"v2\"");
    f.transport.respond(HttpMethod::Head, media_url, head);
    auto get = response(200, "image/jpeg", "0123", "\"v2\"");
    f.transport.respond(HttpMethod::Get, media_url, get);

    auto r = f.download(media_url);

    CHECK(r.status == DownloadStatus::Failed);
    CHECK(r.error == "size 4 instead of 10");
    CHECK(r.local_path == cache_file);
    CHECK(f.content(cache_file) == "old");
    CHECK(f.content(cache_file + ".part") == "0123");
}

TEST_CASE("empty file fails")
{
    Fixture f;
    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", ""));
    f.transport.respond(HttpMethod::Get, media_url, response(200, "image/jpeg", ""));

    auto r = f.download(media_url);

    CHECK(r.status == DownloadStatus::Failed);
    CHECK(r.error == "empty file");
    CHECK_FALSE(f.fs.exists(cache_file + ".part"));
}

TEST_CASE("not enough disk space: no GET, the cached file stays")
{
    Fixture f;
    f.putCached("old", "\"v1\"");
    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "new!", "\"v2\""));

    std::uint64_t    requested = 0;
    DownloaderConfig cfg       = config();
    cfg.make_space             = [&](std::uint64_t bytes)
    {
        requested = bytes;
        return false;
    };
    Downloader downloader{f.transport, f.fs, f.inventory, f.clock, cfg};

    std::optional<DownloadResult> result;
    REQUIRE(downloader.start(media_url, {}, [&](const DownloadResult &r) { result = r; }));
    for (int i = 0; i < 100 && downloader.isBusy(); ++i)
        f.transport.poll();

    REQUIRE(result.has_value());
    CHECK(result->status == DownloadStatus::Failed);
    CHECK(result->local_path == cache_file);
    CHECK(requested == 4);
    CHECK(f.transport.requests.size() == 1);
}

TEST_CASE("idle timeout: a stalled request is cancelled, the cached file stays")
{
    Fixture f;
    f.putCached("old", "\"v1\"");

    std::optional<DownloadResult> result;
    REQUIRE(f.downloader.start(media_url, {}, [&](const DownloadResult &r) { result = r; }));

    // the server does not answer: the transport is not polled
    f.clock.advance(59s);
    f.downloader.poll();
    CHECK_FALSE(result.has_value());

    f.clock.advance(2s);
    f.downloader.poll();

    REQUIRE(result.has_value());
    CHECK(result->status == DownloadStatus::Failed);
    CHECK(result->local_path == cache_file);
    CHECK(f.transport.activeCount() == 0);
}

TEST_CASE("transport refuses the request: reported from poll(), not from start()")
{
    Fixture f;
    f.transport.refuse_start = true;

    std::optional<DownloadResult> result;
    REQUIRE(f.downloader.start(media_url, {}, [&](const DownloadResult &r) { result = r; }));
    CHECK_FALSE(result.has_value());

    f.downloader.poll();
    REQUIRE(result.has_value());
    CHECK(result->status == DownloadStatus::Failed);
    CHECK(result->error == "refused");
}

// ------------------------------------------------------------------ content types

TEST_CASE("web pages are not cacheable")
{
    Fixture f;
    f.transport.respond(HttpMethod::Head, "http://example.com/page", response(200, "text/html; charset=utf-8", ""));

    auto r = f.download("http://example.com/page");

    CHECK(r.status == DownloadStatus::NotCacheable);
    CHECK(r.local_path.empty());
    CHECK(f.transport.requests.size() == 1);
}

TEST_CASE("unknown content type fails, except for .wgt and .smil")
{
    Fixture f;
    f.transport.respond(HttpMethod::Head, "http://example.com/x.bin", response(200, "application/x-unknown", "x"));
    CHECK(f.download("http://example.com/x.bin").status == DownloadStatus::Failed);

    f.transport.respond(HttpMethod::Head, "http://example.com/app.wgt", response(200, "application/x-unknown", "x"));
    f.transport.respond(HttpMethod::Get, "http://example.com/app.wgt", response(200, "application/x-unknown", "x"));
    CHECK(f.download("http://example.com/app.wgt").status == DownloadStatus::Downloaded);
}

TEST_CASE("a file with a fixed name (system file) is taken with any content type")
{
    Fixture f;
    f.transport.respond(HttpMethod::Head, "http://example.com/fw", response(200, "application/octet-stream", "bin"));
    f.transport.respond(HttpMethod::Get, "http://example.com/fw", response(200, "application/octet-stream", "bin"));
    auto firmware = f.download("http://example.com/fw", "garlic-player.tar.xz");
    CHECK(firmware.status == DownloadStatus::Downloaded);
    CHECK(firmware.local_path == "cache/garlic-player.tar.xz");

    f.transport.respond(HttpMethod::Head, "http://example.com/cfg", response(200, "text/html", "<x/>"));
    f.transport.respond(HttpMethod::Get, "http://example.com/cfg", response(200, "text/html", "<x/>"));
    CHECK(f.download("http://example.com/cfg", "configuration.xml").status == DownloadStatus::Downloaded);
}

// ------------------------------------------------------------------ resume

TEST_CASE("aborted download is resumed with Range and If-Range")
{
    Fixture f;
    f.putFile(cache_file + ".part", "01234");
    f.inventory.save({media_url, Md5::hex(media_url) + ".jpg", "\"v1\"", "", "image/jpeg", 10, InventoryState::Transfer});

    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "0123456789", "\"v1\""));
    f.transport.respond(HttpMethod::Get, media_url, response(206, "image/jpeg", "56789", "\"v1\""));

    auto r = f.download(media_url);

    CHECK(r.status == DownloadStatus::Downloaded);
    CHECK(f.content(cache_file) == "0123456789");
    CHECK(f.requestHeader(1, "Range") == "bytes=5-");
    CHECK(f.requestHeader(1, "If-Range") == "\"v1\"");
}

TEST_CASE("server sends the complete file instead of the rest: .part starts again")
{
    Fixture f;
    f.putFile(cache_file + ".part", "xxxxx");
    f.inventory.save({media_url, Md5::hex(media_url) + ".jpg", "\"v1\"", "", "image/jpeg", 10, InventoryState::Transfer});

    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "0123456789", "\"v1\""));
    f.transport.respond(HttpMethod::Get, media_url, response(200, "image/jpeg", "0123456789", "\"v1\""));

    auto r = f.download(media_url);

    CHECK(r.status == DownloadStatus::Downloaded);
    CHECK(f.content(cache_file) == "0123456789");
}

TEST_CASE("no resume if the file changed on the server")
{
    Fixture f;
    f.putFile(cache_file + ".part", "old");
    f.inventory.save({media_url, Md5::hex(media_url) + ".jpg", "\"v1\"", "", "image/jpeg", 10, InventoryState::Transfer});

    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "new!", "\"v2\""));
    f.transport.respond(HttpMethod::Get, media_url, response(200, "image/jpeg", "new!", "\"v2\""));

    auto r = f.download(media_url);

    CHECK_FALSE(f.requestHeader(1, "Range").has_value());
    CHECK(r.status == DownloadStatus::Downloaded);
    CHECK(f.content(cache_file) == "new!");
}

// ------------------------------------------------------------------ usage

TEST_CASE("a busy downloader rejects a second job")
{
    Fixture f;
    REQUIRE(f.downloader.start(media_url, {}, [](const DownloadResult &) {}));
    CHECK_FALSE(f.downloader.start("http://example.com/b.jpg", {}, [](const DownloadResult &) {}));
}

TEST_CASE("the next job can be started from inside the callback")
{
    Fixture     f;
    std::string second = "http://example.com/b.jpg";
    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "a"));
    f.transport.respond(HttpMethod::Get, media_url, response(200, "image/jpeg", "a"));
    f.transport.respond(HttpMethod::Head, second, response(200, "image/jpeg", "b"));
    f.transport.respond(HttpMethod::Get, second, response(200, "image/jpeg", "b"));

    std::vector<std::string> finished;
    REQUIRE(f.downloader.start(media_url, {},
                               [&](const DownloadResult &r)
                               {
                                   finished.push_back(r.url);
                                   REQUIRE(f.downloader.start(second, {}, [&](const DownloadResult &r2)
                                                              { finished.push_back(r2.url); }));
                               }));
    for (int i = 0; i < 100 && f.downloader.isBusy(); ++i)
        f.transport.poll();

    CHECK(finished == std::vector<std::string>{media_url, second});
}

TEST_CASE("cancel() stops the job without callback")
{
    Fixture f;
    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "data"));

    bool called = false;
    REQUIRE(f.downloader.start(media_url, {}, [&](const DownloadResult &) { called = true; }));
    f.downloader.cancel();
    for (int i = 0; i < 10; ++i)
        f.transport.poll();

    CHECK_FALSE(called);
    CHECK_FALSE(f.downloader.isBusy());
    CHECK(f.transport.activeCount() == 0);
}
