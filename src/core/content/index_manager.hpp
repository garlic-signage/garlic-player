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
#ifndef GARLIC_CORE_CONTENT_INDEX_MANAGER_HPP
#define GARLIC_CORE_CONTENT_INDEX_MANAGER_HPP

#include "common/clock.hpp"
#include "common/file_system.hpp"
#include "configuration/configuration.hpp"
#include "downloader/download_queue.hpp"
#include "logger/logger.hpp"
#include "smil/document.hpp"

#include <chrono>
#include <expected>
#include <functional>
#include <optional>
#include <string>

namespace garlic::core::content
{
    /**
     * The SMIL index, garlic-lib: IndexManager. Only valid documents are handed on; a broken index leaves the current
     * one playing.
     *
     *   start()        the last played index from the cache plays at once (also offline), then a check for an update
     *   refresh        meta http-equiv="Refresh" of the index, at least 60 s, 0 = never
     *   update         a new version (index.smil.ready) is parsed first; only if it is valid it replaces the old one
     *   no valid index the manager tries again every 60 s (garlic-lib restarted the player)
     */
    class IndexManager
    {
        public:
            /// A valid document and the base for its relative src (directory or URL ending with "/").
            using OnIndex = std::function<void(smil::Document document, std::string base)>;

            IndexManager(downloader::DownloadQueue &queue, common::FileSystem &fs, const common::Clock &clock,
                         logger::Logger &logger, configuration::Configuration &config, std::string cache_dir,
                         OnIndex on_index);

            void start();
            void poll();

            /// Plays another index for a while (REST /v2/app/exec): not stored as index_uri, not as last played, in
            /// its own file temporary.smil. With its own refresh, until start() goes back to the stored index.
            void startTemporary(std::string uri);

            /// Result from the DownloadQueue for the index (see url()).
            void downloaded(const downloader::DownloadResult &result);

            /// The URL of a remote index, to dispatch download results; empty for a local index.
            const std::string &url() const noexcept { return remote_url; }

            std::optional<std::chrono::steady_clock::time_point> nextDeadline() const { return next_check; }

            static constexpr std::chrono::seconds min_refresh{60};
            static constexpr std::chrono::seconds retry{60};

        private:
            void load(std::string uri);
            downloader::DownloadQueue    &queue;
            common::FileSystem           &fs;
            const common::Clock          &clock;
            logger::Logger               &logger;
            configuration::Configuration &config;
            std::string                   cache_dir;
            OnIndex                       on_index;

            std::string remote_url; // empty: local index
            std::string file_name = "index.smil"; // in the cache
            bool        temporary = false;
            bool        has_index = false;
            bool        source_loaded = false; // an index of the current source (uri) has been handed on
            std::optional<std::chrono::steady_clock::time_point> next_check;

            std::expected<smil::Document, std::string> read(const std::string &path);
            void accept(smil::Document document, const std::string &path);
            void checkIn(std::chrono::seconds delay);
            logger::Metadata metadata(const std::string &path) const;
    };
}

#endif // GARLIC_CORE_CONTENT_INDEX_MANAGER_HPP
