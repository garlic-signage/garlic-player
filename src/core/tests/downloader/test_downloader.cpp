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
#include "failing_file_system.hpp"
#include "fake_clock.hpp"
#include "fake_http_transport.hpp"
#include "memory_inventory.hpp"

#include <array>
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
        garlic::core::tests::FailingFileSystem failing{fs}; // nothing fails unless a test says so
        MemoryInventory   inventory;
        FakeHttpTransport transport;
        Downloader        downloader{transport, failing, inventory, clock, config()};

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

TEST_CASE("file name: no extension from the host, a long one or one with other characters, the type decides")
{
    for (std::string url : {"http://example.com", "http://example.com/a.abcdef", "http://example.com/a.j-g"})
    {
        CAPTURE(url);
        Fixture f;
        f.transport.respond(HttpMethod::Head, url, response(200, "image/png", "data"));
        f.transport.respond(HttpMethod::Get, url, response(200, "image/png", "data"));

        CHECK(f.download(url).local_path == "cache/" + Md5::hex(url) + ".png");
    }

    Fixture     f; // 5 characters are still an extension
    std::string url = "http://example.com/a.abcde";
    f.transport.respond(HttpMethod::Head, url, response(200, "image/png", "data"));
    f.transport.respond(HttpMethod::Get, url, response(200, "image/png", "data"));
    CHECK(f.download(url).local_path == "cache/" + Md5::hex(url) + ".abcde");
}

TEST_CASE("file name: letters a-z, A-Z and digits 0-9 make an extension, in lowercase")
{
    for (auto [url, ext] : {std::pair{"http://example.com/a.az", "az"}, {"http://example.com/a.AZ", "az"},
                            {"http://example.com/a.09", "09"}})
    {
        CAPTURE(url);
        Fixture f;
        f.transport.respond(HttpMethod::Head, url, response(200, "image/png", "data"));
        f.transport.respond(HttpMethod::Get, url, response(200, "image/png", "data"));
        CHECK(f.download(url).local_path == "cache/" + Md5::hex(url) + "." + ext);
    }
}

TEST_CASE("file name: a dot in the query is no extension, a type without extension gives none")
{
    Fixture     f;
    std::string script = "http://example.com/get.php?name=a.gif";
    f.transport.respond(HttpMethod::Head, script, response(200, "image/png", "data"));
    f.transport.respond(HttpMethod::Get, script, response(200, "image/png", "data"));
    CHECK(f.download(script).local_path == "cache/" + Md5::hex(script) + ".png");

    std::string text = "http://example.com/readme";
    f.transport.respond(HttpMethod::Head, text, response(200, "text/plain", "data"));
    f.transport.respond(HttpMethod::Get, text, response(200, "text/plain", "data"));
    CHECK(f.download(text).local_path == "cache/" + Md5::hex(text));
}

TEST_CASE("file name: spaces around the content type are ignored")
{
    Fixture     f;
    std::string url = "http://example.com/media/12345";
    f.transport.respond(HttpMethod::Head, url, response(200, "  IMAGE/PNG ; charset=binary", "data"));
    f.transport.respond(HttpMethod::Get, url, response(200, "image/png", "data"));

    CHECK(f.download(url).local_path == "cache/" + Md5::hex(url) + ".png");
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
    CHECK_FALSE(f.requestHeader(0, "If-Modified-Since").has_value()); // none known: not sent empty
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
    CHECK_FALSE(f.requestHeader(0, "If-None-Match").has_value()); // none known: not sent empty
}

TEST_CASE("server ignores the condition: another ETag or another Last-Modified is a new file")
{
    SUBCASE("another ETag, the same Last-Modified and size")
    {
        Fixture f;
        f.putFile(cache_file, "old");
        f.inventory.save({media_url, Md5::hex(media_url) + ".jpg", "\"v1\"", "Wed, 01 Jan 2025 10:00:00 GMT",
                          "image/jpeg", 3, InventoryState::Complete});
        auto head = response(200, "image/jpeg", "new", "\"v2\"");
        head.headers.add("Last-Modified", "Wed, 01 Jan 2025 10:00:00 GMT");
        f.transport.respond(HttpMethod::Head, media_url, head);
        f.transport.respond(HttpMethod::Get, media_url, response(200, "image/jpeg", "new"));
        CHECK(f.download(media_url).status == DownloadStatus::Downloaded);
    }
    SUBCASE("no ETag, another Last-Modified, the same size")
    {
        Fixture f;
        f.putFile(cache_file, "old");
        f.inventory.save({media_url, Md5::hex(media_url) + ".jpg", "", "Wed, 01 Jan 2025 10:00:00 GMT", "image/jpeg",
                          3, InventoryState::Complete});
        auto head = response(200, "image/jpeg", "new");
        head.headers.add("Last-Modified", "Thu, 02 Jan 2025 10:00:00 GMT");
        f.transport.respond(HttpMethod::Head, media_url, head);
        f.transport.respond(HttpMethod::Get, media_url, response(200, "image/jpeg", "new"));
        CHECK(f.download(media_url).status == DownloadStatus::Downloaded);
    }
}

TEST_CASE("without ETag the same Last-Modified with another size is a new file")
{
    Fixture f;
    f.putFile(cache_file, "old");
    f.inventory.save({media_url, Md5::hex(media_url) + ".jpg", "", "Wed, 01 Jan 2025 10:00:00 GMT", "image/jpeg", 3,
                      InventoryState::Complete});

    auto head = response(200, "image/jpeg", "new!");
    head.headers.add("Last-Modified", "Wed, 01 Jan 2025 10:00:00 GMT");
    f.transport.respond(HttpMethod::Head, media_url, head);
    f.transport.respond(HttpMethod::Get, media_url, response(200, "image/jpeg", "new!"));

    auto r = f.download(media_url);

    CHECK(r.status == DownloadStatus::Downloaded);
    CHECK(f.transport.requests.size() == 2);
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

    // a slash in the query of the request is not part of its path
    std::string query = "http://example.com/media/get?file=x/y";
    f.transport.respond(HttpMethod::Head, query, redirect);
    CHECK(f.download(query).status == DownloadStatus::Downloaded);
    CHECK(f.transport.requests.back().url == "http://example.com/media/b.jpg");
}

TEST_CASE("redirect without scheme (//host/path) keeps the scheme of the request")
{
    Fixture f;
    auto    redirect = response(302, "", "");
    redirect.headers.add("Location", "//cdn.example.com/a.jpg");
    f.transport.respond(HttpMethod::Head, media_url, redirect);
    f.transport.respond(HttpMethod::Head, "http://cdn.example.com/a.jpg", response(200, "image/jpeg", "data"));
    f.transport.respond(HttpMethod::Get, "http://cdn.example.com/a.jpg", response(200, "image/jpeg", "data"));

    CHECK(f.download(media_url).status == DownloadStatus::Downloaded);
    CHECK(f.transport.requests.at(1).url == "http://cdn.example.com/a.jpg");
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
    f.putFile("cache/other.jpg", "other"); // the cache directory is there
    auto    r = f.download(media_url); // fake answers 404

    CHECK(r.status == DownloadStatus::Failed);
    CHECK(r.http_status == 404);
    CHECK(r.local_path.empty());
}

TEST_CASE("HTTP error for a file with a fixed name: the copy in the cache is returned")
{
    Fixture f;
    f.putFile("cache/index.smil", "<smil/>"); // not in the inventory
    f.transport.respond(HttpMethod::Head, "http://example.com/index.smil", response(500, "", ""));

    auto r = f.download("http://example.com/index.smil", "index.smil");
    CHECK(r.status == DownloadStatus::Failed);
    CHECK(r.local_path == "cache/index.smil");
}

TEST_CASE("the connection fails during HEAD: failed with the error of the transport")
{
    Fixture f;
    auto    head = response(200, "image/jpeg", "data");
    head.error   = TransportError{TransportErrorCode::ConnectionFailed, "connection reset"};
    f.transport.respond(HttpMethod::Head, media_url, head);

    auto r = f.download(media_url);
    CHECK(r.status == DownloadStatus::Failed);
    CHECK(r.error == "connection reset");
    CHECK(f.transport.requests.size() == 1);
}

TEST_CASE("without Content-Length any size is taken")
{
    Fixture f;
    auto    r = response(200, "image/jpeg", "data");
    r.headers = {};
    r.headers.add("Content-Type", "image/jpeg");
    f.transport.respond(HttpMethod::Head, media_url, r);
    f.transport.respond(HttpMethod::Get, media_url, r);

    CHECK(f.download(media_url).status == DownloadStatus::Downloaded);
    CHECK(f.content(cache_file) == "data");
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

    f.transport.respond(HttpMethod::Head, "http://example.com/x", response(200, "application/xhtml+xml", ""));
    CHECK(f.download("http://example.com/x").status == DownloadStatus::NotCacheable);
}

TEST_CASE("supported content types: image, video, audio, text and some applications")
{
    for (std::string type : {"audio/mpeg", "text/plain", "application/smil", "application/smil+xml",
                             "application/xml", "application/zip", "application/widget",
                             "application/vnd.android.package-archive"})
    {
        CAPTURE(type);
        Fixture f;
        f.transport.respond(HttpMethod::Head, "http://example.com/x.bin", response(200, type, "x"));
        f.transport.respond(HttpMethod::Get, "http://example.com/x.bin", response(200, type, "x"));
        CHECK(f.download("http://example.com/x.bin").status == DownloadStatus::Downloaded);
    }
}

TEST_CASE("unknown content type fails, except for .wgt and .smil")
{
    Fixture f;
    f.transport.respond(HttpMethod::Head, "http://example.com/x.bin", response(200, "application/x-unknown", "x"));
    auto unknown = f.download("http://example.com/x.bin");
    CHECK(unknown.status == DownloadStatus::Failed);
    CHECK(unknown.error == "unsupported content type: application/x-unknown");
    CHECK(f.transport.requests.size() == 1); // no GET

    f.transport.respond(HttpMethod::Head, "http://example.com/app.wgt", response(200, "application/x-unknown", "x"));
    f.transport.respond(HttpMethod::Get, "http://example.com/app.wgt", response(200, "application/x-unknown", "x"));
    CHECK(f.download("http://example.com/app.wgt").status == DownloadStatus::Downloaded);

    f.transport.respond(HttpMethod::Head, "http://example.com/i.smil", response(200, "application/x-unknown", "x"));
    f.transport.respond(HttpMethod::Get, "http://example.com/i.smil", response(200, "application/x-unknown", "x"));
    CHECK(f.download("http://example.com/i.smil").status == DownloadStatus::Downloaded);
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

TEST_CASE("no resume without ETag if Last-Modified changed")
{
    Fixture f;
    f.putFile(cache_file + ".part", "01234");
    f.inventory.save({media_url, Md5::hex(media_url) + ".jpg", "", "Wed, 01 Jan 2025 10:00:00 GMT", "image/jpeg", 10,
                      InventoryState::Transfer});
    auto head = response(200, "image/jpeg", "abcdefghij");
    head.headers.add("Last-Modified", "Thu, 02 Jan 2025 10:00:00 GMT");
    f.transport.respond(HttpMethod::Head, media_url, head);
    f.transport.respond(HttpMethod::Get, media_url, response(200, "image/jpeg", "abcdefghij"));

    CHECK(f.download(media_url).status == DownloadStatus::Downloaded);
    CHECK_FALSE(f.requestHeader(1, "Range").has_value());
    CHECK(f.content(cache_file) == "abcdefghij");
}

TEST_CASE("resume without ETag: Last-Modified is the If-Range validator")
{
    Fixture f;
    f.putFile(cache_file + ".part", "01234");
    f.inventory.save({media_url, Md5::hex(media_url) + ".jpg", "", "Wed, 01 Jan 2025 10:00:00 GMT", "image/jpeg", 10,
                      InventoryState::Transfer});

    auto head = response(200, "image/jpeg", "0123456789");
    head.headers.add("Last-Modified", "Wed, 01 Jan 2025 10:00:00 GMT");
    f.transport.respond(HttpMethod::Head, media_url, head);
    f.transport.respond(HttpMethod::Get, media_url, response(206, "image/jpeg", "56789"));

    auto r = f.download(media_url);

    CHECK(r.status == DownloadStatus::Downloaded);
    CHECK(f.content(cache_file) == "0123456789");
    CHECK(f.requestHeader(1, "Range") == "bytes=5-");
    CHECK(f.requestHeader(1, "If-Range") == "Wed, 01 Jan 2025 10:00:00 GMT");
}

TEST_CASE("no resume with a weak ETag or an empty .part file")
{
    SUBCASE("weak ETag: not allowed in If-Range")
    {
        Fixture f;
        f.putFile(cache_file + ".part", "01234");
        f.inventory.save({media_url, Md5::hex(media_url) + ".jpg", "W/\"v1\"", "Wed, 01 Jan 2025 10:00:00 GMT",
                          "image/jpeg", 10, InventoryState::Transfer});
        auto head = response(200, "image/jpeg", "0123456789", "W/\"v1\"");
        head.headers.add("Last-Modified", "Wed, 01 Jan 2025 10:00:00 GMT"); // not taken instead of the weak ETag
        f.transport.respond(HttpMethod::Head, media_url, head);
        f.transport.respond(HttpMethod::Get, media_url, response(200, "image/jpeg", "0123456789", "W/\"v1\""));

        CHECK(f.download(media_url).status == DownloadStatus::Downloaded);
        CHECK_FALSE(f.requestHeader(1, "Range").has_value());
    }
    SUBCASE("empty .part file")
    {
        Fixture f;
        f.putFile(cache_file + ".part", "");
        f.inventory.save({media_url, Md5::hex(media_url) + ".jpg", "\"v1\"", "", "image/jpeg", 10,
                          InventoryState::Transfer});
        f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "0123456789", "\"v1\""));
        f.transport.respond(HttpMethod::Get, media_url, response(200, "image/jpeg", "0123456789", "\"v1\""));

        CHECK(f.download(media_url).status == DownloadStatus::Downloaded);
        CHECK_FALSE(f.requestHeader(1, "Range").has_value());
    }
}

TEST_CASE("416 range not satisfiable: the .part file is deleted, the next attempt starts from the beginning")
{
    Fixture f;
    f.putFile(cache_file + ".part", "0123456789xx");
    f.inventory.save({media_url, Md5::hex(media_url) + ".jpg", "\"v1\"", "", "image/jpeg", 10,
                      InventoryState::Transfer});
    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "0123456789", "\"v1\""));
    f.transport.respond(HttpMethod::Get, media_url, response(416, "", ""));

    auto r = f.download(media_url);

    CHECK(r.status == DownloadStatus::Failed);
    CHECK(f.requestHeader(1, "Range") == "bytes=12-");
    CHECK_FALSE(f.fs.exists(cache_file + ".part"));
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

TEST_CASE("cancel() during GET and the destructor stop the request at the transport")
{
    Fixture f;
    auto    slow = response(200, "image/jpeg", "0123456789");
    slow.chunk_size = 1;
    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "0123456789"));
    f.transport.respond(HttpMethod::Get, media_url, slow);
    REQUIRE(f.downloader.start(media_url, {}, [](const DownloadResult &) {}));
    for (int i = 0; i < 100 && f.transport.requests.size() < 2; ++i)
        f.transport.poll();
    REQUIRE(f.transport.requests.size() == 2); // the GET runs
    f.downloader.cancel();
    CHECK(f.transport.activeCount() == 0);

    {
        Downloader other{f.transport, f.fs, f.inventory, f.clock, config()};
        REQUIRE(other.start(media_url, {}, [](const DownloadResult &) {}));
        CHECK(f.transport.activeCount() == 1);
    }
    CHECK(f.transport.activeCount() == 0);
}

TEST_CASE("a new job starts clean: no error and no status of the one before")
{
    Fixture f;
    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "data"));
    f.transport.respond(HttpMethod::Get, media_url, response(200, "image/jpeg", "data"));

    // refused and cancelled before poll() reported it
    f.transport.refuse_start = true;
    REQUIRE(f.downloader.start(media_url, {}, [](const DownloadResult &) {}));
    f.downloader.cancel();
    f.transport.refuse_start = false;
    CHECK(f.download(media_url).status == DownloadStatus::Downloaded);

    // a write error
    f.failing.fail_write = true;
    REQUIRE(f.download("http://example.com/b.jpg").status == DownloadStatus::Failed); // 404, no write
    f.transport.respond(HttpMethod::Head, "http://example.com/b.jpg", response(200, "image/jpeg", "b"));
    f.transport.respond(HttpMethod::Get, "http://example.com/b.jpg", response(200, "image/jpeg", "b"));
    REQUIRE(f.download("http://example.com/b.jpg").error == "write failed");
    f.failing.fail_write = false;
    CHECK(f.download("http://example.com/b.jpg").status == DownloadStatus::Downloaded);

    // a refused start has no HTTP status, also after a 404
    REQUIRE(f.download("http://example.com/missing.jpg").http_status == 404);
    f.transport.refuse_start = true;
    CHECK(f.download(media_url).http_status == 0);
}

TEST_CASE("the GET is refused: failed with the error of the transport")
{
    Fixture f;
    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "data"));
    DownloaderConfig c = config();
    c.make_space       = [&](std::uint64_t) { return f.transport.refuse_start = true; }; // right before the GET
    Downloader downloader{f.transport, f.failing, f.inventory, f.clock, c};

    std::optional<DownloadResult> result;
    REQUIRE(downloader.start(media_url, {}, [&](const DownloadResult &r) { result = r; }));
    for (int i = 0; i < 100 && downloader.isBusy(); ++i)
        f.transport.poll();
    REQUIRE(result.has_value());
    CHECK(result->status == DownloadStatus::Failed);
    CHECK(result->error == "refused");
}

// ------------------------------------------------------------------ errors of the file system

TEST_CASE("the .part file cannot be opened, written or closed: failed, the cached copy stays")
{
    for (int step = 0; step < 3; ++step)
    {
        CAPTURE(step);
        Fixture f;
        f.putCached("old", "\"v1\"");
        f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "new!", "\"v2\""));
        f.transport.respond(HttpMethod::Get, media_url, response(200, "image/jpeg", "new!", "\"v2\""));
        f.failing.fail_open  = step == 0;
        f.failing.fail_write = step == 1;
        f.failing.fail_close = step == 2;

        auto result = f.download(media_url);
        CHECK(result.status == DownloadStatus::Failed);
        CHECK(result.error == std::array{"open failed", "write failed", "close failed"}[step]);
        CHECK(result.local_path == cache_file); // the old copy plays on
        CHECK(f.content(cache_file) == "old");
        CHECK_FALSE(f.fs.exists(cache_file + ".ready"));
        CHECK(f.inventory.find(media_url)->state == InventoryState::Transfer); // the next try loads it again
    }
}

TEST_CASE("the .part file cannot be written and not closed: the first error counts")
{
    Fixture f;
    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "data"));
    f.transport.respond(HttpMethod::Get, media_url, response(200, "image/jpeg", "data"));
    f.failing.fail_write = true;
    f.failing.fail_close = true;
    CHECK(f.download(media_url).error == "write failed");
}

TEST_CASE("the finished .part file cannot be renamed: failed, not in the inventory as complete")
{
    Fixture f;
    f.transport.respond(HttpMethod::Head, media_url, response(200, "image/jpeg", "data", "\"v1\""));
    f.transport.respond(HttpMethod::Get, media_url, response(200, "image/jpeg", "data", "\"v1\""));
    f.failing.fail_rename = true;

    auto result = f.download(media_url);
    CHECK(result.status == DownloadStatus::Failed);
    CHECK(result.error == "rename failed");
    CHECK(result.local_path.empty());
    CHECK_FALSE(f.fs.exists(cache_file));
    auto entry = f.inventory.find(media_url);
    CHECK((!entry || entry->state != InventoryState::Complete));
}
