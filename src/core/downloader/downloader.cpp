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
#include "downloader/downloader.hpp"

#include "common/md5.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <utility>

namespace garlic::core::downloader
{
    namespace
    {
        constexpr std::string_view part_suffix  = ".part";
        constexpr std::string_view ready_suffix = ".ready";

        std::string toLower(std::string_view text)
        {
            std::string result(text);
            for (char &c : result)
            {
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
            }
            return result;
        }

        bool isAlnum(char c) noexcept
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        }

        bool hasScheme(std::string_view url)
        {
            auto pos = url.find("://");
            return pos != std::string_view::npos && url.substr(0, pos).find_first_of("/?#") == std::string_view::npos;
        }

        /// Resolves the Location header of a redirect against the URL that was requested.
        std::string resolveUrl(std::string_view base, std::string_view location)
        {
            if (hasScheme(location) || !hasScheme(base))
                return std::string(location);

            auto scheme_end = base.find("://");
            if (location.starts_with("//"))
                return std::string(base.substr(0, scheme_end + 1)) + std::string(location);

            auto             host_end = base.find('/', scheme_end + 3);
            std::string_view origin   = host_end == std::string_view::npos ? base : base.substr(0, host_end);
            if (location.starts_with('/'))
                return std::string(origin) + std::string(location);

            // relative path: replaces the last segment of the base path
            std::string_view path = host_end == std::string_view::npos ? "/" : base.substr(host_end);
            path                  = path.substr(0, path.find_first_of("?#"));
            return std::string(origin) + std::string(path.substr(0, path.rfind('/') + 1)) + std::string(location);
        }

        /// File extension of the URL path in lowercase, empty if there is none or it is a script (file.php?id=…).
        std::string urlExtension(std::string_view url)
        {
            std::string_view path = url.substr(0, url.find_first_of("?#"));
            if (hasScheme(path))
            {
                auto slash = path.find('/', path.find("://") + 3);
                path       = slash == std::string_view::npos ? std::string_view{} : path.substr(slash);
            }

            std::string_view name = path.substr(path.rfind('/') + 1);
            auto             dot  = name.rfind('.');
            if (dot == std::string_view::npos || dot + 1 == name.size())
                return {};

            std::string_view ext = name.substr(dot + 1);
            if (ext.size() > 5 || !std::ranges::all_of(ext, isAlnum))
                return {};

            std::string lower = toLower(ext);
            constexpr std::array<std::string_view, 8> scripts = {"php", "asp", "aspx", "jsp", "cgi", "pl", "py", "do"};
            if (std::ranges::find(scripts, lower) != scripts.end())
                return {};

            return lower;
        }

        /// "image/JPEG; charset=x" → "image/jpeg"
        std::string mediaType(std::string_view content_type)
        {
            std::string_view type = content_type.substr(0, content_type.find(';'));
            while (!type.empty() && type.back() == ' ')
                type.remove_suffix(1);
            while (!type.empty() && type.front() == ' ')
                type.remove_prefix(1);
            return toLower(type);
        }

        bool isWebPage(std::string_view type)
        {
            return type == "text/html" || type == "application/xhtml+xml";
        }

        bool isSupportedType(std::string_view type)
        {
            constexpr std::array<std::string_view, 6> applications = {
                "application/smil", "application/smil+xml", "application/xml",
                "application/zip",  "application/widget",   "application/vnd.android.package-archive"};

            return type.starts_with("image/") || type.starts_with("video/") || type.starts_with("audio/") ||
                   type.starts_with("text/") || std::ranges::find(applications, type) != applications.end();
        }

        /// Files the old player accepted by extension even with an unknown content type.
        bool isSupportedExtension(std::string_view ext)
        {
            return ext == "wgt" || ext == "smil";
        }

        std::string extensionForType(std::string_view type)
        {
            constexpr std::array<std::pair<std::string_view, std::string_view>, 22> types = {{
                {"image/jpeg", "jpg"},
                {"image/png", "png"},
                {"image/gif", "gif"},
                {"image/webp", "webp"},
                {"image/bmp", "bmp"},
                {"image/svg+xml", "svg"},
                {"video/mp4", "mp4"},
                {"video/webm", "webm"},
                {"video/quicktime", "mov"},
                {"video/x-matroska", "mkv"},
                {"video/mpeg", "mpg"},
                {"audio/mpeg", "mp3"},
                {"audio/ogg", "ogg"},
                {"audio/wav", "wav"},
                {"audio/aac", "aac"},
                {"application/smil", "smil"},
                {"application/smil+xml", "smil"},
                {"application/xml", "xml"},
                {"text/xml", "xml"},
                {"application/zip", "zip"},
                {"application/widget", "wgt"},
                {"application/vnd.android.package-archive", "apk"},
            }};

            auto it = std::ranges::find(types, type, &std::pair<std::string_view, std::string_view>::first);
            return it == types.end() ? std::string{} : std::string(it->second);
        }

        std::uint64_t parseLength(std::optional<std::string_view> value)
        {
            std::uint64_t result = 0;
            if (value)
                std::from_chars(value->data(), value->data() + value->size(), result);
            return result;
        }

        bool isRedirect(int status)
        {
            return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
        }
    }

    Downloader::Downloader(HttpTransport &t, common::FileSystem &f, Inventory &i, const common::Clock &c,
                           DownloaderConfig cfg)
        : transport(t), fs(f), inventory(i), clock(c), config(std::move(cfg))
    {
    }

    Downloader::~Downloader()
    {
        cancel();
    }

    bool Downloader::start(std::string job_url, std::string job_file_name, FinishedCallback callback)
    {
        if (isBusy())
            return false;

        url         = std::move(job_url);
        current_url = url;
        file_name   = std::move(job_file_name);
        on_finished = std::move(callback);
        known       = inventory.find(url);
        remote      = {};
        redirects   = 0;
        http_status = 0;
        start_error.clear();

        sendHead();
        return true;
    }

    void Downloader::poll()
    {
        if (!isBusy())
            return;

        // on_finished must not be called from start(), so a failed start is reported here
        if (!start_error.empty())
        {
            finish(DownloadStatus::Failed, std::exchange(start_error, {}));
            return;
        }

        if (clock.monotonic() - last_activity > config.idle_timeout)
        {
            transport.cancel(request_id);
            finish(DownloadStatus::Failed, "timeout: no data received");
        }
    }

    void Downloader::cancel()
    {
        if (!isBusy())
            return;

        transport.cancel(request_id);
        closeFile();
        phase       = Phase::Idle;
        on_finished = nullptr;
    }

    void Downloader::sendHead()
    {
        HttpRequest request{HttpMethod::Head, current_url, {}};
        request.headers.add("User-Agent", config.user_agent);

        // Conditional request only if the cached file still exists, otherwise a 304 would leave us without a file.
        if (known && known->state == InventoryState::Complete && !cachedCopy().empty())
        {
            if (!known->etag.empty())
                request.headers.add("If-None-Match", known->etag);
            if (!known->last_modified.empty())
                request.headers.add("If-Modified-Since", known->last_modified);
        }

        phase         = Phase::Head;
        last_activity = clock.monotonic();
        head          = {};

        auto id = transport.start(request, *this);
        if (!id)
        {
            start_error = id.error().message;
            return;
        }
        request_id = *id;
    }

    void Downloader::sendGet()
    {
        HttpRequest request{HttpMethod::Get, current_url, {}};
        request.headers.add("User-Agent", config.user_agent);

        // Resume an aborted download, but only if the server still has the same version (If-Range).
        auto part_size = fs.fileSize(partPath());
        if (part_size && *part_size > 0 && known && known->state == InventoryState::Transfer)
        {
            std::string validator;
            if (!remote.etag.empty() && remote.etag == known->etag && !remote.etag.starts_with("W/"))
                validator = remote.etag;
            else if (remote.etag.empty() && !remote.last_modified.empty() &&
                     remote.last_modified == known->last_modified)
                validator = remote.last_modified;

            if (!validator.empty())
            {
                request.headers.add("Range", "bytes=" + std::to_string(*part_size) + "-");
                request.headers.add("If-Range", validator);
            }
        }

        phase         = Phase::Get;
        last_activity = clock.monotonic();
        head          = {};
        write_error.clear();

        auto id = transport.start(request, *this);
        if (!id)
        {
            finish(DownloadStatus::Failed, id.error().message);
            return;
        }
        request_id = *id;
    }

    void Downloader::onHead(const HttpResponseHead &response)
    {
        last_activity = clock.monotonic();
        head          = response;
        http_status   = response.status;

        if (phase != Phase::Get)
            return;

        if (response.status == 206 || response.status == 200)
        {
            // 206: server continues the .part file, 200: complete file (no or outdated .part)
            auto opened = fs.openForWrite(partPath(), response.status == 206 ? common::WriteMode::Append
                                                                             : common::WriteMode::Truncate);
            if (opened)
                file = std::move(*opened);
            else
                write_error = opened.error().message;
        }
        else if (response.status == 416)
        {
            // range not satisfiable: the .part file does not fit anymore, the next attempt starts from the beginning
            (void)fs.remove(partPath());
        }
    }

    bool Downloader::onData(std::span<const std::byte> chunk)
    {
        last_activity = clock.monotonic();
        if (!file)
            return false; // error status: the body is not needed

        if (auto written = file->write(chunk); !written)
        {
            write_error = written.error().message;
            return false;
        }
        return true;
    }

    void Downloader::onComplete(std::expected<void, TransportError> result)
    {
        if (phase == Phase::Head)
        {
            if (!result)
                finish(DownloadStatus::Failed, result.error().message);
            else
                evaluateHead();
            return;
        }

        closeFile();
        if (!write_error.empty())
            finish(DownloadStatus::Failed, write_error);
        else if (head.status != 0 && head.status != 200 && head.status != 206)
            finish(DownloadStatus::Failed, "HTTP " + std::to_string(head.status));
        else if (!result)
            finish(DownloadStatus::Failed, result.error().message);
        else
            finishGet();
    }

    void Downloader::evaluateHead()
    {
        if (isRedirect(head.status))
        {
            auto location = head.headers.find("Location");
            if (!location)
            {
                finish(DownloadStatus::Failed, "redirect without Location");
                return;
            }
            if (++redirects > config.max_redirects)
            {
                finish(DownloadStatus::Failed, "too many redirects");
                return;
            }
            current_url = resolveUrl(current_url, *location);
            sendHead();
            return;
        }

        if (head.status == 304)
        {
            if (file_name.empty() && known)
                file_name = known->file_name;
            finish(DownloadStatus::NotModified);
            return;
        }

        if (head.status != 200)
        {
            finish(DownloadStatus::Failed, "HTTP " + std::to_string(head.status));
            return;
        }

        std::string type    = mediaType(head.headers.find("Content-Type").value_or(""));
        std::string url_ext = urlExtension(url);
        if (isWebPage(type))
        {
            finish(DownloadStatus::NotCacheable);
            return;
        }
        if (!isSupportedType(type) && !isSupportedExtension(url_ext))
        {
            finish(DownloadStatus::Failed, "unsupported content type: " + type);
            return;
        }

        if (file_name.empty())
        {
            std::string ext = url_ext.empty() || type == "application/octet-stream" ? extensionForType(type) : url_ext;
            if (ext.empty())
                ext = url_ext;
            file_name = common::Md5::hex(url) + (ext.empty() ? "" : "." + ext);
        }

        remote.url            = url;
        remote.file_name      = file_name;
        remote.etag           = std::string(head.headers.find("ETag").value_or(""));
        remote.last_modified  = std::string(head.headers.find("Last-Modified").value_or(""));
        remote.content_type   = type;
        remote.content_length = parseLength(head.headers.find("Content-Length"));

        // Servers that ignore conditional requests: compare with what we know
        if (known && known->state == InventoryState::Complete && fs.exists(targetPath()))
        {
            bool same_etag = !remote.etag.empty() && remote.etag == known->etag;
            bool same_date = remote.etag.empty() && !remote.last_modified.empty() &&
                             remote.last_modified == known->last_modified &&
                             remote.content_length == known->content_length;
            if (same_etag || same_date)
            {
                finish(DownloadStatus::NotModified);
                return;
            }
        }

        if (config.make_space && !config.make_space(remote.content_length))
        {
            finish(DownloadStatus::Failed, "not enough disk space");
            return;
        }

        remote.state = InventoryState::Transfer;
        inventory.save(remote);
        sendGet();
    }

    void Downloader::finishGet()
    {
        auto size = fs.fileSize(partPath());
        if (!size || *size == 0)
        {
            (void)fs.remove(partPath());
            finish(DownloadStatus::Failed, "empty file");
            return;
        }

        if (remote.content_length > 0 && *size != remote.content_length)
        {
            // too small: keep the .part file for resuming; too big: it is broken
            if (*size > remote.content_length)
                (void)fs.remove(partPath());
            finish(DownloadStatus::Failed, "size " + std::to_string(*size) + " instead of " +
                                               std::to_string(remote.content_length));
            return;
        }

        // Never overwrite the existing file, it may be playing: the new one waits as .ready
        std::string destination = targetPath();
        if (fs.exists(destination))
        {
            destination += ready_suffix;
            (void)fs.remove(destination);
        }

        if (auto renamed = fs.rename(partPath(), destination); !renamed)
        {
            finish(DownloadStatus::Failed, renamed.error().message);
            return;
        }

        remote.state = InventoryState::Complete;
        inventory.save(remote);
        finish(DownloadStatus::Downloaded);
    }

    void Downloader::finish(DownloadStatus status, std::string error)
    {
        closeFile();

        DownloadResult result;
        result.status      = status;
        result.url         = url;
        result.error       = std::move(error);
        result.http_status = http_status;

        switch (status)
        {
            case DownloadStatus::Downloaded:
            case DownloadStatus::NotModified:
                result.local_path = targetPath();
                break;
            case DownloadStatus::Failed:
                result.local_path = cachedCopy();
                break;
            case DownloadStatus::NotCacheable:
                break;
        }

        // Idle before the callback, so it can start the next job right away
        phase         = Phase::Idle;
        auto callback = std::move(on_finished);
        on_finished   = nullptr;
        if (callback)
            callback(result);
    }

    void Downloader::closeFile()
    {
        if (!file)
            return;

        if (auto closed = file->close(); !closed && write_error.empty())
            write_error = closed.error().message;
        file.reset();
    }

    std::string Downloader::targetPath() const
    {
        return config.cache_dir + "/" + file_name;
    }

    std::string Downloader::partPath() const
    {
        return targetPath() + std::string(part_suffix);
    }

    std::string Downloader::cachedCopy() const
    {
        if (!file_name.empty() && fs.exists(targetPath()))
            return targetPath();

        if (known && !known->file_name.empty())
        {
            std::string path = config.cache_dir + "/" + known->file_name;
            if (fs.exists(path))
                return path;
        }
        return {};
    }
}
