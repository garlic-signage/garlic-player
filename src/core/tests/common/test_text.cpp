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
#include <doctest/doctest.h>

#include "common/text.hpp"

using garlic::core::common::toLower;

TEST_CASE("toLower changes only A-Z")
{
    CHECK(toLower("Content-Type") == "content-type");
    CHECK(toLower("IMAGE/JPEG; Charset=UTF-8") == "image/jpeg; charset=utf-8");
    CHECK(toLower("AZ@[") == "az@["); // both ends of A-Z, the characters next to them stay
    CHECK(toLower("\xC3\x84rger 123") == "\xC3\x84rger 123"); // UTF-8 stays as it is
}
