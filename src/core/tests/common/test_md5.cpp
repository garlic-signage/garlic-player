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

#include "common/md5.hpp"

#include <string>

using garlic::core::common::Md5;

TEST_CASE("MD5 matches the test suite of RFC 1321")
{
    CHECK(Md5::hex("") == "d41d8cd98f00b204e9800998ecf8427e");
    CHECK(Md5::hex("a") == "0cc175b9c0f1b6a831c399e269772661");
    CHECK(Md5::hex("abc") == "900150983cd24fb0d6963f7d28e17f72");
    CHECK(Md5::hex("message digest") == "f96b697d7cb7938d525a2f31aaf161d0");
    CHECK(Md5::hex("abcdefghijklmnopqrstuvwxyz") == "c3fcd3d76192e4007dfb496cca67e13b");
    CHECK(Md5::hex("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") ==
          "d174ab98d277d9f5a5611c2c9f419d9f");
    CHECK(Md5::hex("12345678901234567890123456789012345678901234567890123456789012345678901234567890") ==
          "57edf4a22be3c955ac49da2e2107b67a");
}

TEST_CASE("MD5 handles the padding at block boundaries")
{
    // 55, 56 and 64 bytes: last byte before the length field, length field in the next block, exactly one block
    CHECK(Md5::hex(std::string(55, 'a')) == "ef1772b6dff9a122358552954ad0df65");
    CHECK(Md5::hex(std::string(56, 'a')) == "3b0c8ac703f828b04c6c197006d17218");
    CHECK(Md5::hex(std::string(64, 'a')) == "014842d480b571495a4a0363793f7367");
}

TEST_CASE("MD5 in chunks gives the same result as in one piece")
{
    const std::string text(1000, 'x');

    Md5 md5;
    for (std::size_t offset = 0; offset < text.size(); offset += 7)
        md5.update(std::string_view(text).substr(offset, 7));

    CHECK(Md5::toHex(md5.finish()) == Md5::hex(text));
}

TEST_CASE("MD5 starts again after finish()")
{
    Md5 md5;
    md5.update("something");
    (void)md5.finish();

    md5.update("abc");
    CHECK(Md5::toHex(md5.finish()) == "900150983cd24fb0d6963f7d28e17f72");
}

TEST_CASE("MD5 of an URL as used for cache file names")
{
    CHECK(Md5::hex("http://example.com/media/a.jpg") == "a629c75a14ced75a90812e4fb46d70e8");
}
