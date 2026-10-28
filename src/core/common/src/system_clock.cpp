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
#include "garlic/core/common/clock.hpp"

namespace garlic::core::common
{
    std::chrono::system_clock::time_point SystemClock::now() const noexcept
    {
        return std::chrono::system_clock::now();
    }

    std::chrono::steady_clock::time_point SystemClock::monotonic() const noexcept
    {
        return std::chrono::steady_clock::now();
    }
}
