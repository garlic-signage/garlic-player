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
// other systems (e.g. BSD): no values, the reports stay empty there
#include "system/platform.hpp"

namespace garlic::core::system::platform
{
    Memory memory()
    {
        return {};
    }

    ProcessInfo process()
    {
        return {};
    }

    std::vector<reports::NetworkInterface> network()
    {
        return {};
    }

    std::string timeZone()
    {
        return {};
    }

    std::string locale()
    {
        return "C";
    }
}
