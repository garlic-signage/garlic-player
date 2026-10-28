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
#include "content/media_manager.hpp"

#include "content/widget.hpp"

namespace garlic::core::content
{
    namespace
    {
        bool isRemote(std::string_view src)
        {
            return src.starts_with("http://") || src.starts_with("https://") || src.starts_with("ftp://");
        }

        /// Used as it is: adapi commands, data: URIs, absolute local paths.
        bool isDirect(std::string_view src)
        {
            return src.starts_with("adapi:") || src.starts_with("data:") || src.starts_with('/') ||
                   (src.size() > 2 && src[1] == ':'); // Windows drive
        }

        std::string_view kindName(smil::MediaKind kind)
        {
            switch (kind)
            {
                case smil::MediaKind::Image: return "image";
                case smil::MediaKind::Video: return "video";
                case smil::MediaKind::Audio: return "audio";
                case smil::MediaKind::Web: return "text";
                case smil::MediaKind::Widget: return "widget";
                case smil::MediaKind::Command: return "command";
                case smil::MediaKind::Brush: return "brush";
                case smil::MediaKind::Prefetch: return "prefetch";
            }
            return "unknown";
        }
    }

    MediaManager::MediaManager(downloader::DownloadQueue &q, const downloader::Inventory &inv, common::FileSystem &f,
                               logger::Logger &log, std::string cache, Supports can_play, MakeSpace space)
        : queue(q), inventory(inv), fs(f), logger(log), cache_dir(std::move(cache)), supports(std::move(can_play)),
          make_space(std::move(space))
    {
    }

    // ------------------------------------------------------------------ new index

    void MediaManager::load(const smil::Document &document, std::string_view index_base)
    {
        base = index_base;
        reported.clear(); // MEDIA_UNSUPPORTED again once per index version

        registerSrc(document.head.root_layout.background_image);
        for (const smil::Region &region : document.head.regions)
            registerSrc(region.background_image);
        registerMedia(document.body);
    }

    void MediaManager::registerMedia(const smil::Container &container)
    {
        for (const smil::Node &node : container.children)
        {
            if (const auto *child = std::get_if<smil::Container>(&node))
            {
                registerMedia(*child);
                continue;
            }
            const auto &media = std::get<smil::Media>(node);
            if (media.kind == smil::MediaKind::Brush)
                continue;
            if (supports && !supports(media))
            {
                reportUnsupported(media);
                continue;
            }
            registerSrc(media.src);
        }
    }

    /// Remote files go to the download queue; a local widget is extracted (garlic-lib: registerFile).
    void MediaManager::registerSrc(std::string_view src)
    {
        if (src.empty() || src == "none" || src == "inherited")
            return;
        std::string path = absolute(src);
        if (isRemote(path))
            queue.add(path);
        else if (path.ends_with(".wgt") && !path.starts_with("adapi:"))
        {
            if (auto extracted = extractWidget(fs, path, make_space); !extracted)
                logger.warning("ContentManager", path + ": " + extracted.error());
        }
    }

    /// garlic-lib reported unsupported media nowhere; now an event the hub can show, once per index version.
    void MediaManager::reportUnsupported(const smil::Media &media)
    {
        if (!reported.insert(media.src).second)
            return;
        logger.event(logger::Severity::Warning, "ContentManager", "MEDIA_UNSUPPORTED",
                     {{"contentId", media.log_content_id}, {"resourceURI", media.src},
                      {"mediaType", std::string(kindName(media.kind))}});
    }

    void MediaManager::downloaded(const downloader::DownloadResult &result)
    {
        if (result.status == downloader::DownloadStatus::NotCacheable)
            not_cacheable.insert(result.url);
        // everything else is in the inventory; an update waits as ".ready" until the file is free
    }

    // ------------------------------------------------------------------ for the engine

    /**
     * garlic-lib: getLoadablePath and checkCacheStatus.
     *   - not supported or not yet downloaded: nullopt, the engine skips it and tries again next time
     *   - not cacheable (e.g. a web page): the URL itself, unless cacheControl="onlyIfCached"
     *   - an updated file (".ready") replaces the old one when no instance plays it any more
     *   - widgets: the extracted index.html
     */
    std::optional<std::string> MediaManager::resolve(const smil::Media &media)
    {
        if (supports && !supports(media))
            return std::nullopt;
        std::string path = absolute(media.src);
        if (path.empty())
            return std::nullopt;

        if (isRemote(path))
        {
            if (not_cacheable.contains(path))
            {
                if (media.cache_control == smil::CacheControl::OnlyIfCached)
                    return std::nullopt;
                return path;
            }
            auto file = cachedFile(path);
            if (!file)
                return std::nullopt;
            ++in_use[*file];
            holding[&media] = *file;
            if (media.kind == smil::MediaKind::Widget)
                return widgetIndex(*file);
            return file;
        }

        if (media.kind == smil::MediaKind::Widget && path.ends_with(".wgt"))
            return widgetIndex(path);
        return path;
    }

    void MediaManager::release(const smil::Media &media)
    {
        auto held = holding.find(&media);
        if (held == holding.end())
            return;
        if (--in_use[held->second] <= 0)
            in_use.erase(held->second);
        holding.erase(held);
    }

    /// garlic-lib: checkForUpdate.
    void MediaManager::prefetch(const smil::Media &media)
    {
        std::string path = absolute(media.src);
        if (isRemote(path))
            queue.add(path);
    }

    std::optional<std::string> MediaManager::locate(std::string_view src)
    {
        std::string path = absolute(src);
        if (!isRemote(path))
            return path;
        if (not_cacheable.contains(path))
            return path;
        return cachedFile(path);
    }

    // ------------------------------------------------------------------ helpers

    /// "img/a.jpg" against the index base; remote and direct src stay as they are.
    std::string MediaManager::absolute(std::string_view src) const
    {
        if (src.starts_with("file://"))
            src.remove_prefix(7);
        if (src.empty() || isRemote(src) || isDirect(src))
            return std::string(src);
        return base + std::string(src);
    }

    /**
     * The cache file of a downloaded URL, from the inventory of the downloader. A waiting update (".ready") replaces it
     * first if no instance plays it; a widget is extracted.
     */
    std::optional<std::string> MediaManager::cachedFile(std::string_view url)
    {
        auto entry = inventory.find(url);
        if (!entry || entry->file_name.empty())
            return std::nullopt;
        std::string file  = cache_dir + "/" + entry->file_name;
        std::string ready = file + ".ready";

        bool updated = false;
        if (!in_use.contains(file) && fs.exists(ready))
            updated = fs.rename(ready, file).has_value();
        if (!fs.exists(file))
            return std::nullopt;

        if (file.ends_with(".wgt") && (updated || !fs.exists(widgetIndex(file))))
        {
            if (auto extracted = extractWidget(fs, file, make_space); !extracted)
                logger.warning("ContentManager", file + ": " + extracted.error());
            if (!fs.exists(widgetIndex(file)))
                return std::nullopt;
        }
        return file;
    }
}
