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
#include "content/widget.hpp"

#include <miniz.h>

#include <memory>
#include <vector>

namespace garlic::core::content
{
    namespace
    {
        struct ZipDeleter
        {
            void operator()(mz_zip_archive *zip) const { mz_zip_reader_end(zip); }
        };

        /// An entry name that stays inside the widget directory: relative, without "..".
        bool isSafe(std::string_view name)
        {
            if (name.empty() || name.front() == '/' || name.front() == '\\' || name.contains(':'))
                return false;
            std::size_t start = 0;
            while (start <= name.size())
            {
                std::size_t end  = name.find_first_of("/\\", start);
                auto        part = name.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
                if (part == "..")
                    return false;
                if (end == std::string_view::npos)
                    break;
                start = end + 1;
            }
            return true;
        }

        std::string parentOf(std::string_view path)
        {
            auto slash = path.rfind('/');
            return slash == std::string_view::npos ? std::string() : std::string(path.substr(0, slash));
        }

        /// garlic-lib: Wgt::mustExtract - the directory is up to date if its index.html is not older than the file.
        bool isUpToDate(const common::FileSystem &fs, std::string_view wgt_path)
        {
            auto wgt   = fs.lastModified(wgt_path);
            auto index = fs.lastModified(widgetIndex(wgt_path));
            return wgt && index && *index >= *wgt;
        }
    }

    std::string widgetDirectory(std::string_view wgt_path)
    {
        // the whole name without ".wgt"; garlic-lib compared with baseName(), which cuts at the first dot, so a
        // widget like "xyz.secret.wgt" was extracted again at every check
        if (wgt_path.ends_with(".wgt"))
            wgt_path.remove_suffix(4);
        return std::string(wgt_path);
    }

    std::string widgetIndex(std::string_view wgt_path)
    {
        return widgetDirectory(wgt_path) + "/index.html";
    }

    std::expected<void, std::string> extractWidget(common::FileSystem &fs, std::string_view wgt_path,
                                                   const std::function<bool(std::uint64_t)> &make_space)
    {
        if (isUpToDate(fs, wgt_path))
            return {};

        auto data = fs.readFile(wgt_path);
        if (!data)
            return std::unexpected("cannot read widget: " + data.error().message);

        mz_zip_archive archive{};
        if (!mz_zip_reader_init_mem(&archive, data->data(), data->size(), 0))
            return std::unexpected("widget is no zip archive");
        std::unique_ptr<mz_zip_archive, ZipDeleter> zip(&archive);

        mz_uint count = mz_zip_reader_get_num_files(zip.get());
        if (make_space)
        {
            std::uint64_t unpacked = 0; // garlic-lib: calculateUncompressedSize
            for (mz_uint i = 0; i < count; ++i)
                if (mz_zip_archive_file_stat stat; mz_zip_reader_file_stat(zip.get(), i, &stat))
                    unpacked += stat.m_uncomp_size;
            if (!make_space(unpacked))
                return std::unexpected("not enough disk space to extract the widget");
        }

        std::string directory = widgetDirectory(wgt_path);
        std::string part      = directory + ".part";
        (void)fs.removeAll(part);
        if (auto created = fs.createDirectories(part); !created)
            return std::unexpected("cannot create " + part + ": " + created.error().message);

        auto fail = [&](std::string reason) -> std::expected<void, std::string>
        {
            (void)fs.removeAll(part);
            return std::unexpected(std::move(reason));
        };

        for (mz_uint i = 0; i < count; ++i)
        {
            mz_zip_archive_file_stat stat;
            if (!mz_zip_reader_file_stat(zip.get(), i, &stat))
                return fail("broken zip entry");
            std::string_view name = stat.m_filename;
            if (!isSafe(name))
                return fail("unsafe path in widget: " + std::string(name));

            std::string target = part + "/" + std::string(name);
            if (mz_zip_reader_is_file_a_directory(zip.get(), i))
            {
                (void)fs.createDirectories(target);
                continue;
            }

            std::size_t size    = 0;
            void       *content = mz_zip_reader_extract_to_heap(zip.get(), i, &size, 0);
            if (content == nullptr)
                return fail("cannot extract " + std::string(name));
            std::unique_ptr<void, decltype(&mz_free)> owned(content, &mz_free);

            (void)fs.createDirectories(parentOf(target));
            auto file = fs.openForWrite(target, common::WriteMode::Truncate);
            if (!file)
                return fail("cannot write " + target + ": " + file.error().message);
            auto bytes = std::span(static_cast<const std::byte *>(content), size);
            if (auto written = (*file)->write(bytes); !written)
                return fail("cannot write " + target + ": " + written.error().message);
            if (auto closed = (*file)->close(); !closed)
                return fail("cannot write " + target + ": " + closed.error().message);
        }

        // only now the old version goes
        (void)fs.removeAll(directory);
        if (auto renamed = fs.rename(part, directory); !renamed)
            return fail("cannot replace " + directory + ": " + renamed.error().message);
        return {};
    }
}
