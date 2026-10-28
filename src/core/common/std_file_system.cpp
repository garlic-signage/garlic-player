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
#include "common/std_file_system.hpp"

#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <system_error>

namespace garlic::core::common
{
    namespace
    {
        namespace fs = std::filesystem;

        // Paths are UTF-8. A plain std::string would be interpreted in the ANSI code page on Windows.
        fs::path toPath(std::string_view path)
        {
            return fs::path(std::u8string(path.begin(), path.end()));
        }

        FileSystemError toError(const std::error_code &ec, std::string_view path)
        {
            FileSystemErrorCode code = FileSystemErrorCode::IoError;
            if (ec == std::errc::no_such_file_or_directory)
                code = FileSystemErrorCode::NotFound;
            else if (ec == std::errc::permission_denied || ec == std::errc::operation_not_permitted)
                code = FileSystemErrorCode::PermissionDenied;
            else if (ec == std::errc::no_space_on_device)
                code = FileSystemErrorCode::NoSpace;

            return {code, std::string(path) + ": " + ec.message()};
        }

        FileSystemError errnoError(std::string_view path)
        {
            return toError(std::error_code(errno, std::generic_category()), path);
        }

        std::FILE *openFile(const fs::path &path, WriteMode mode)
        {
#ifdef _WIN32
            return _wfopen(path.c_str(), mode == WriteMode::Append ? L"ab" : L"wb");
#else
            return std::fopen(path.c_str(), mode == WriteMode::Append ? "ab" : "wb");
#endif
        }

        class StdWritableFile final : public WritableFile
        {
            public:
                StdWritableFile(std::FILE *f, std::string p) : file(f), path(std::move(p)) {}

                ~StdWritableFile() override
                {
                    if (file != nullptr)
                        std::fclose(file);
                }

                FileSystemResult<void> write(std::span<const std::byte> data) override
                {
                    if (file == nullptr)
                        return std::unexpected(FileSystemError{FileSystemErrorCode::IoError, path + ": closed"});

                    if (std::fwrite(data.data(), 1, data.size(), file) != data.size())
                        return std::unexpected(errnoError(path));

                    return {};
                }

                FileSystemResult<void> close() override
                {
                    if (file == nullptr)
                        return {};

                    int result = std::fclose(file);
                    file       = nullptr;
                    if (result != 0)
                        return std::unexpected(errnoError(path));

                    return {};
                }

            private:
                std::FILE  *file;
                std::string path;
        };
    }

    bool StdFileSystem::exists(std::string_view path) const
    {
        std::error_code ec;
        return fs::exists(toPath(path), ec);
    }

    FileSystemResult<std::uint64_t> StdFileSystem::fileSize(std::string_view path) const
    {
        std::error_code ec;
        auto            size = fs::file_size(toPath(path), ec);
        if (ec)
            return std::unexpected(toError(ec, path));

        return static_cast<std::uint64_t>(size);
    }

    FileSystemResult<std::chrono::system_clock::time_point> StdFileSystem::lastModified(std::string_view path) const
    {
        std::error_code ec;
        auto            time = fs::last_write_time(toPath(path), ec);
        if (ec)
            return std::unexpected(toError(ec, path));

        return std::chrono::time_point_cast<std::chrono::system_clock::duration>(std::chrono::file_clock::to_sys(time));
    }

    FileSystemResult<std::unique_ptr<WritableFile>> StdFileSystem::openForWrite(std::string_view path, WriteMode mode)
    {
        std::FILE *file = openFile(toPath(path), mode);
        if (file == nullptr)
            return std::unexpected(errnoError(path));

        return std::make_unique<StdWritableFile>(file, std::string(path));
    }

    FileSystemResult<void> StdFileSystem::rename(std::string_view from, std::string_view to)
    {
        std::error_code ec;
        fs::rename(toPath(from), toPath(to), ec);
        if (ec)
            return std::unexpected(toError(ec, from));

        return {};
    }

    FileSystemResult<void> StdFileSystem::remove(std::string_view path)
    {
        std::error_code ec;
        if (!fs::remove(toPath(path), ec))
        {
            if (!ec)
                ec = std::make_error_code(std::errc::no_such_file_or_directory);
            return std::unexpected(toError(ec, path));
        }

        return {};
    }

    FileSystemResult<void> StdFileSystem::createDirectories(std::string_view path)
    {
        std::error_code ec;
        fs::create_directories(toPath(path), ec);
        if (ec)
            return std::unexpected(toError(ec, path));

        return {};
    }
}
