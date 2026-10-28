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
#ifndef GARLIC_CORE_COMMON_FILE_SYSTEM_HPP
#define GARLIC_CORE_COMMON_FILE_SYSTEM_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace garlic::core::common
{
    enum class FileSystemErrorCode
    {
        NotFound,
        PermissionDenied,
        NoSpace,
        IoError
    };

    struct FileSystemError
    {
        FileSystemErrorCode code;
        std::string         message; // for logging only
    };

    template <typename T> using FileSystemResult = std::expected<T, FileSystemError>;

    enum class WriteMode
    {
        Truncate, // create or empty the file
        Append    // create or continue at the end (resume of downloads)
    };

    /// File opened for writing. Closed by close() or at the latest by the destructor.
    class WritableFile
    {
        public:
            virtual ~WritableFile() = default;

            virtual FileSystemResult<void> write(std::span<const std::byte> data) = 0;

            /// Flushes and closes the file. Errors here mean the data may not be stored completely.
            virtual FileSystemResult<void> close() = 0;
    };

    /**
     * Access to files, implemented per platform (std::filesystem, OPFS, Tizen, webOS, LittleFS/SD) and in memory.
     *
     * Paths are UTF-8 strings with '/' as separator.
     */
    class FileSystem
    {
        public:
            virtual ~FileSystem() = default;

            virtual bool exists(std::string_view path) const = 0;

            virtual FileSystemResult<std::uint64_t> fileSize(std::string_view path) const = 0;

            virtual FileSystemResult<std::chrono::system_clock::time_point> lastModified(std::string_view path) const = 0;

            /// Complete content of a file. For small files like the configuration or SMIL indexes.
            virtual FileSystemResult<std::string> readFile(std::string_view path) const = 0;

            virtual FileSystemResult<std::unique_ptr<WritableFile>> openForWrite(std::string_view path, WriteMode mode) = 0;

            /// Renames a file. An existing target is replaced.
            virtual FileSystemResult<void> rename(std::string_view from, std::string_view to) = 0;

            /// Removes a file. Returns NotFound if it does not exist.
            virtual FileSystemResult<void> remove(std::string_view path) = 0;

            /// Creates a directory including all missing parents. Existing directories are no error.
            virtual FileSystemResult<void> createDirectories(std::string_view path) = 0;
    };
}

#endif // GARLIC_CORE_COMMON_FILE_SYSTEM_HPP
