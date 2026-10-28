/*************************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2026 Nikolaos Saghiadinos <ns@smil-control.com>
    This file is part of the garlic-player source code

    This program is free software: you can redistribute it and/or  modify
    it under the terms of the GNU Affero General Public License, version 3,
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*************************************************************************************/
#ifndef GARLIC_CORE_DOWNLOADER_DOWNLOADER_H
#define GARLIC_CORE_DOWNLOADER_DOWNLOADER_H

#include <string_view>

namespace garlic::core::downloader
{
    /// Placeholder until the download queue is designed. Keeps the static module buildable and testable.
    std::string_view moduleName() noexcept;
}

#endif // GARLIC_CORE_DOWNLOADER_DOWNLOADER_H
