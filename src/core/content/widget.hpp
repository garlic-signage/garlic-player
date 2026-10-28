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
#ifndef GARLIC_CORE_CONTENT_WIDGET_HPP
#define GARLIC_CORE_CONTENT_WIDGET_HPP

#include "common/file_system.hpp"

#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <string_view>

namespace garlic::core::content
{
    /// The directory a widget is extracted to: "cache/abc.wgt" → "cache/abc" (next to the file, like garlic-lib).
    std::string widgetDirectory(std::string_view wgt_path);

    /// The page that is played: "cache/abc.wgt" → "cache/abc/index.html".
    std::string widgetIndex(std::string_view wgt_path);

    /**
     * Extracts a widget (a zip archive) if its directory is missing or older than the file, garlic-lib: Wgt.
     *
     * Everything is extracted into "<directory>.part" first; only when that worked it replaces the directory. So a
     * broken file keeps the old widget, like a failed download keeps the old file. Entries with an absolute path or
     * ".." are not allowed ("zip slip"): such an archive is rejected as a whole.
     *
     * make_space (CacheSpace::makeSpace) gets the unpacked size first; false: not extracted.
     */
    std::expected<void, std::string> extractWidget(common::FileSystem &fs, std::string_view wgt_path,
                                                   const std::function<bool(std::uint64_t)> &make_space = {});
}

#endif // GARLIC_CORE_CONTENT_WIDGET_HPP
