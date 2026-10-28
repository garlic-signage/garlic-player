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
#include "content/index_manager.hpp"

#include "common/time_format.hpp"
#include "smil/parser.hpp"

#include <algorithm>

namespace garlic::core::content
{
    namespace
    {
        bool isRemote(std::string_view uri)
        {
            return uri.starts_with("http://") || uri.starts_with("https://") || uri.starts_with("ftp://");
        }
    }

    IndexManager::IndexManager(downloader::DownloadQueue &q, common::FileSystem &f, const common::Clock &c,
                               logger::Logger &log, configuration::Configuration &cfg, std::string cache,
                               OnIndex callback)
        : queue(q), fs(f), clock(c), logger(log), config(cfg), cache_dir(std::move(cache)), on_index(std::move(callback))
    {
    }

    /**
     * Remote index: the last played one from the cache first, so the screen is not empty while the network is slow
     * or down (garlic-lib: initParser), then the download. Local index (path, USB): read directly.
     */
    void IndexManager::start()
    {
        temporary = false;
        file_name = "index.smil";
        load(config.indexUri());
    }

    void IndexManager::startTemporary(std::string uri)
    {
        temporary = true;
        file_name = "temporary.smil";
        load(std::move(uri));
    }

    void IndexManager::load(std::string uri)
    {
        next_check.reset();
        source_loaded = false;
        if (uri.starts_with("file://"))
            uri.erase(0, 7);

        if (!isRemote(uri))
        {
            remote_url.clear();
            if (auto document = read(uri))
                accept(std::move(*document), uri);
            return;
        }

        remote_url        = uri;
        std::string cache = cache_dir + "/" + file_name;
        std::string last  = config.lastPlayedIndexPath();
        if (!temporary && last == cache && fs.exists(cache))
        {
            if (auto document = read(cache))
                accept(std::move(*document), cache);
        }
        queue.add(remote_url, file_name);
    }

    void IndexManager::poll()
    {
        if (next_check && clock.monotonic() >= *next_check)
        {
            next_check.reset();
            if (!remote_url.empty())
                queue.add(remote_url, file_name);
        }
    }

    /**
     * garlic-lib: doSucceed / doNotModified. An update waits as index.smil.ready next to the playing index and is
     * parsed first: only a valid one replaces it. A broken one is deleted, the current index plays on.
     */
    void IndexManager::downloaded(const downloader::DownloadResult &result)
    {
        using downloader::DownloadStatus;
        if (result.status == DownloadStatus::Downloaded && !result.local_path.empty())
        {
            std::string ready = result.local_path + ".ready";
            if (fs.exists(ready))
            {
                auto document = read(ready);
                if (!document)
                {
                    (void)fs.remove(ready);
                    checkIn(has_index ? min_refresh : retry);
                    return;
                }
                if (auto renamed = fs.rename(ready, result.local_path); !renamed)
                {
                    logger.error("ContentManager", "cannot replace index: " + renamed.error().message);
                    checkIn(retry);
                    return;
                }
                accept(std::move(*document), result.local_path);
                return;
            }
        }

        // first download, not modified, or failed with a copy in the cache: play it if nothing of this source plays yet
        if (!source_loaded && !result.local_path.empty() && result.status != DownloadStatus::NotCacheable)
        {
            if (auto document = read(result.local_path))
            {
                accept(std::move(*document), result.local_path);
                return;
            }
        }
        if (result.status == DownloadStatus::Failed)
            logger.warning("ContentManager", "index download failed: " + result.error);
        if (!next_check)
            checkIn(has_index ? min_refresh : retry);
    }

    // ------------------------------------------------------------------ helpers

    /// Reads and parses an index; errors become events like in garlic-lib (SMIL_NOT_AVAILABLE, SMIL_OPEN_ERROR).
    std::expected<smil::Document, std::string> IndexManager::read(const std::string &path)
    {
        auto text = fs.readFile(path);
        if (!text)
        {
            logger.event(logger::Severity::Informational, "ContentManager", "SMIL_NOT_AVAILABLE",
                         {{"instanceId", "display:0"}, {"resourceURI", path}});
            return std::unexpected(text.error().message);
        }
        auto document = smil::parse(*text);
        if (!document)
        {
            auto data = metadata(path);
            data.emplace_back("errorMessage", document.error().message);
            data.emplace_back("errorLine", std::to_string(document.error().line));
            logger.event(logger::Severity::Error, "ContentManager", "SMIL_OPEN_ERROR", data);
            return std::unexpected(document.error().message);
        }
        return std::move(*document);
    }

    /// A valid index: remembered as last played, handed on, next check after its refresh time.
    void IndexManager::accept(smil::Document document, const std::string &path)
    {
        logger.event(logger::Severity::Informational, "ContentManager", "SMIL_LOADED", metadata(path));
        if (!temporary)
            config.setLastPlayedIndexPath(path);
        has_index     = true;
        source_loaded = true;

        // relative src are resolved against where the index comes from (garlic-lib: getIndexPath)
        std::string source = remote_url.empty() ? path : remote_url;
        source             = source.substr(0, source.find_first_of("?#"));
        auto        slash  = source.rfind('/');
        std::string base   = slash == std::string::npos ? std::string() : source.substr(0, slash + 1);

        std::chrono::seconds refresh = document.head.refresh;
        on_index(std::move(document), std::move(base));

        next_check.reset();
        if (!remote_url.empty() && refresh > std::chrono::seconds(0))
            checkIn(std::max(refresh, min_refresh));
    }

    void IndexManager::checkIn(std::chrono::seconds delay)
    {
        if (!remote_url.empty())
            next_check = clock.monotonic() + delay;
    }

    logger::Metadata IndexManager::metadata(const std::string &path) const
    {
        logger::Metadata data{{"instanceId", "display:0"}, {"resourceURI", path}};
        if (auto size = fs.fileSize(path))
            data.emplace_back("contentLength", std::to_string(*size));
        if (auto modified = fs.lastModified(path))
            data.emplace_back("lastModifiedTime", common::isoLocalTime(*modified));
        return data;
    }
}
