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
#ifndef GARLIC_CORE_TESTS_FAILING_FILE_SYSTEM_HPP
#define GARLIC_CORE_TESTS_FAILING_FILE_SYSTEM_HPP

#include "common/memory_file_system.hpp"

#include <memory>
#include <string>
#include <vector>

namespace garlic::core::tests
{
    using common::FileSystemError;
    using common::FileSystemErrorCode;
    using common::FileSystemResult;

    /// A MemoryFileSystem whose operations can fail (as IoError with a fixed message), and that counts opened files
    /// and created directories. For the error paths MemoryFileSystem cannot reach.
    class FailingFileSystem : public common::FileSystem
    {
        public:
            explicit FailingFileSystem(common::MemoryFileSystem &m) : memory(m) {}

            bool fail_read = false, fail_open = false, fail_write = false, fail_close = false, fail_rename = false;
            int  opened    = 0;
            std::vector<std::string> created_directories;

            bool exists(std::string_view path) const override { return memory.exists(path); }
            FileSystemResult<std::uint64_t> fileSize(std::string_view path) const override { return memory.fileSize(path); }
            FileSystemResult<std::chrono::system_clock::time_point> lastModified(std::string_view path) const override
            {
                return memory.lastModified(path);
            }
            FileSystemResult<std::chrono::system_clock::time_point> lastAccess(std::string_view path) const override
            {
                return memory.lastAccess(path);
            }
            FileSystemResult<std::vector<common::DirectoryEntry>> list(std::string_view path) const override
            {
                return memory.list(path);
            }
            FileSystemResult<std::string> readFile(std::string_view path) const override
            {
                if (fail_read)
                    return error("read failed");
                return memory.readFile(path);
            }
            FileSystemResult<std::unique_ptr<common::WritableFile>> openForWrite(std::string_view path,
                                                                                               common::WriteMode mode) override
            {
                ++opened;
                if (fail_open)
                    return error("open failed");
                auto file = memory.openForWrite(path, mode);
                if (!file)
                    return std::unexpected(file.error());
                return std::make_unique<File>(std::move(*file), *this);
            }
            FileSystemResult<void> rename(std::string_view from, std::string_view to) override
            {
                if (fail_rename)
                    return error("rename failed");
                return memory.rename(from, to);
            }
            FileSystemResult<void> remove(std::string_view path) override { return memory.remove(path); }
            FileSystemResult<void> removeAll(std::string_view path) override { return memory.removeAll(path); }
            FileSystemResult<void> createDirectories(std::string_view path) override
            {
                created_directories.emplace_back(path);
                return memory.createDirectories(path);
            }

        private:
            class File : public common::WritableFile
            {
                public:
                    File(std::unique_ptr<common::WritableFile> f, const FailingFileSystem &o)
                        : file(std::move(f)), owner(o) {}
                    FileSystemResult<void> write(std::span<const std::byte> data) override
                    {
                        if (owner.fail_write)
                            return error("write failed");
                        return file->write(data);
                    }
                    FileSystemResult<void> close() override
                    {
                        auto closed = file->close();
                        if (owner.fail_close)
                            return error("close failed");
                        return closed;
                    }

                private:
                    std::unique_ptr<common::WritableFile> file;
                    const FailingFileSystem                            &owner;
            };

            static std::unexpected<FileSystemError> error(std::string message)
            {
                return std::unexpected(FileSystemError{FileSystemErrorCode::IoError, std::move(message)});
            }

            common::MemoryFileSystem &memory;
    };
}

#endif // GARLIC_CORE_TESTS_FAILING_FILE_SYSTEM_HPP
