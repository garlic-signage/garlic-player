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
#ifndef GARLIC_CORE_COMMON_MEMORY_FILE_SYSTEM_HPP
#define GARLIC_CORE_COMMON_MEMORY_FILE_SYSTEM_HPP

#include "common/clock.hpp"
#include "common/file_system.hpp"

#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace garlic::core::common
{
    /**
     * FileSystem that keeps all files in RAM. For tests and the preview in garlic-hub, where nothing has to be stored
     * persistently. Directories are not modelled: every path is a file.
     */
    class MemoryFileSystem final : public FileSystem
    {
        public:
            explicit MemoryFileSystem(const Clock &c) : clock(c) {}

            bool exists(std::string_view path) const override;
            FileSystemResult<std::uint64_t> fileSize(std::string_view path) const override;
            FileSystemResult<std::chrono::system_clock::time_point> lastModified(std::string_view path) const override;
            FileSystemResult<std::string> readFile(std::string_view path) const override;
            FileSystemResult<std::unique_ptr<WritableFile>> openForWrite(std::string_view path, WriteMode mode) override;
            FileSystemResult<void> rename(std::string_view from, std::string_view to) override;
            FileSystemResult<void> remove(std::string_view path) override;
            FileSystemResult<void> removeAll(std::string_view path) override;
            FileSystemResult<void> createDirectories(std::string_view path) override;

            /// Content of a file as string, std::nullopt if it does not exist.
            std::optional<std::string> content(std::string_view path) const;

            /// Number of files.
            std::size_t fileCount() const noexcept { return files.size(); }

            struct Node
            {
                std::vector<std::byte>                data;
                std::chrono::system_clock::time_point modified;
            };

        private:
            const Clock                                                &clock;
            std::map<std::string, std::shared_ptr<Node>, std::less<>> files;
    };
}

#endif // GARLIC_CORE_COMMON_MEMORY_FILE_SYSTEM_HPP
