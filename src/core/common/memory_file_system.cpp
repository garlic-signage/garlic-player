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
#include "common/memory_file_system.hpp"

namespace garlic::core::common
{
    namespace
    {
        FileSystemError notFound(std::string_view path)
        {
            return {FileSystemErrorCode::NotFound, std::string(path) + ": not found"};
        }

        class MemoryWritableFile final : public WritableFile
        {
            public:
                MemoryWritableFile(std::shared_ptr<MemoryFileSystem::Node> n, const Clock &c)
                    : node(std::move(n)), clock(c)
                {
                }

                FileSystemResult<void> write(std::span<const std::byte> data) override
                {
                    if (!node)
                        return std::unexpected(FileSystemError{FileSystemErrorCode::IoError, "closed"});

                    node->data.insert(node->data.end(), data.begin(), data.end());
                    node->modified = clock.now();
                    return {};
                }

                FileSystemResult<void> close() override
                {
                    node.reset();
                    return {};
                }

            private:
                std::shared_ptr<MemoryFileSystem::Node> node;
                const Clock                            &clock;
        };
    }

    bool MemoryFileSystem::exists(std::string_view path) const
    {
        return files.find(path) != files.end();
    }

    FileSystemResult<std::uint64_t> MemoryFileSystem::fileSize(std::string_view path) const
    {
        auto it = files.find(path);
        if (it == files.end())
            return std::unexpected(notFound(path));

        return static_cast<std::uint64_t>(it->second->data.size());
    }

    FileSystemResult<std::chrono::system_clock::time_point> MemoryFileSystem::lastModified(std::string_view path) const
    {
        auto it = files.find(path);
        if (it == files.end())
            return std::unexpected(notFound(path));

        return it->second->modified;
    }

    FileSystemResult<std::string> MemoryFileSystem::readFile(std::string_view path) const
    {
        auto text = content(path);
        if (!text)
            return std::unexpected(notFound(path));
        return *text;
    }

    FileSystemResult<std::unique_ptr<WritableFile>> MemoryFileSystem::openForWrite(std::string_view path,
                                                                                   WriteMode        mode)
    {
        auto it = files.find(path);
        if (it == files.end())
            it = files.emplace(std::string(path), std::make_shared<Node>()).first;
        else if (mode == WriteMode::Truncate)
            it->second->data.clear();

        it->second->modified = clock.now();
        return std::make_unique<MemoryWritableFile>(it->second, clock);
    }

    FileSystemResult<void> MemoryFileSystem::rename(std::string_view from, std::string_view to)
    {
        auto it = files.find(from);
        if (it == files.end())
            return std::unexpected(notFound(from));

        auto node = it->second;
        files.erase(it);
        files.insert_or_assign(std::string(to), std::move(node));
        return {};
    }

    FileSystemResult<void> MemoryFileSystem::remove(std::string_view path)
    {
        auto it = files.find(path);
        if (it == files.end())
            return std::unexpected(notFound(path));

        files.erase(it);
        return {};
    }

    FileSystemResult<void> MemoryFileSystem::createDirectories(std::string_view)
    {
        return {};
    }

    std::optional<std::string> MemoryFileSystem::content(std::string_view path) const
    {
        auto it = files.find(path);
        if (it == files.end())
            return std::nullopt;

        std::string result;
        result.reserve(it->second->data.size());
        for (std::byte b : it->second->data)
            result.push_back(static_cast<char>(b));
        return result;
    }
}
