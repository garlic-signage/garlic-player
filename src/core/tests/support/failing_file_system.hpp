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
    /// and created directories. For the error paths MemoryFileSystem cannot reach. With "only" set, only paths that
    /// contain it fail (for rename: the source, which must end with it), so e.g. the logs of a test go on working.
    class FailingFileSystem : public common::FileSystem
    {
        public:
            explicit FailingFileSystem(common::MemoryFileSystem &m) : memory(m) {}

            bool fail_read = false, fail_open = false, fail_write = false, fail_close = false, fail_rename = false,
                 fail_remove = false, fail_create_directories = false;
            std::string              only;
            int                      opened = 0;
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
                if (fail_read && hits(path))
                    return error("read failed");
                return memory.readFile(path);
            }
            FileSystemResult<std::unique_ptr<common::WritableFile>> openForWrite(std::string_view path,
                                                                                 common::WriteMode mode) override
            {
                ++opened;
                if (fail_open && hits(path))
                    return error("open failed");
                auto file = memory.openForWrite(path, mode);
                if (!file)
                    return std::unexpected(file.error());
                return std::make_unique<File>(std::move(*file), *this, std::string(path));
            }
            FileSystemResult<void> rename(std::string_view from, std::string_view to) override
            {
                if (fail_rename && (only.empty() || from.ends_with(only))) // "cache/clock" fails, "cache/clock.part" not
                    return error("rename failed");
                return memory.rename(from, to);
            }
            FileSystemResult<void> remove(std::string_view path) override
            {
                if (fail_remove && hits(path))
                    return error("remove failed");
                return memory.remove(path);
            }
            FileSystemResult<void> removeAll(std::string_view path) override
            {
                if (fail_remove && hits(path))
                    return error("remove failed");
                return memory.removeAll(path);
            }
            FileSystemResult<void> createDirectories(std::string_view path) override
            {
                created_directories.emplace_back(path);
                if (fail_create_directories && hits(path))
                    return error("create directories failed");
                return memory.createDirectories(path);
            }

        private:
            class File : public common::WritableFile
            {
                public:
                    File(std::unique_ptr<common::WritableFile> f, const FailingFileSystem &o, std::string p)
                        : file(std::move(f)), owner(o), path(std::move(p))
                    {
                    }
                    FileSystemResult<void> write(std::span<const std::byte> data) override
                    {
                        if (owner.fail_write && owner.hits(path))
                            return error("write failed");
                        return file->write(data);
                    }
                    FileSystemResult<void> close() override
                    {
                        auto closed = file->close();
                        if (owner.fail_close && owner.hits(path))
                            return error("close failed");
                        return closed;
                    }

                private:
                    std::unique_ptr<common::WritableFile> file;
                    const FailingFileSystem              &owner;
                    std::string                           path;
            };

            bool hits(std::string_view path) const { return only.empty() || path.contains(only); }

            static std::unexpected<FileSystemError> error(std::string message)
            {
                return std::unexpected(FileSystemError{FileSystemErrorCode::IoError, std::move(message)});
            }

            common::MemoryFileSystem &memory;
    };
}

#endif // GARLIC_CORE_TESTS_FAILING_FILE_SYSTEM_HPP
