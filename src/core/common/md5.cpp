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
#include "common/md5.hpp"

#include <bit>

namespace garlic::core::common
{
    namespace
    {
        // Per-round shift amounts (RFC 1321, 3.4)
        constexpr std::array<int, 64> shifts = {
            7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
            5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
            4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
            6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

        // floor(abs(sin(i + 1)) * 2^32)
        constexpr std::array<std::uint32_t, 64> constants = {
            0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
            0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
            0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
            0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
            0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
            0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
            0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
            0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    }

    Md5::Md5() noexcept
    {
        reset();
    }

    void Md5::reset() noexcept
    {
        state    = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
        buffer   = {};
        buffered = 0;
        length   = 0;
    }

    void Md5::update(std::span<const std::byte> data) noexcept
    {
        length += data.size();
        for (std::byte b : data)
        {
            buffer[buffered++] = static_cast<std::uint8_t>(b);
            if (buffered == buffer.size())
            {
                transform(buffer.data());
                buffered = 0;
            }
        }
    }

    void Md5::update(std::string_view text) noexcept
    {
        update(std::as_bytes(std::span(text.data(), text.size())));
    }

    Md5::Digest Md5::finish() noexcept
    {
        const std::uint64_t bit_length = length * 8;

        // Padding: 0x80, zeros until 56 bytes in the block, then the length as 64 bit little endian
        constexpr std::byte one{0x80};
        constexpr std::byte zero{0x00};
        update(std::span(&one, 1));
        while (buffered != 56)
            update(std::span(&zero, 1));

        std::array<std::byte, 8> length_bytes;
        for (std::size_t i = 0; i < 8; ++i)
            length_bytes[i] = static_cast<std::byte>(bit_length >> (8 * i));
        update(length_bytes);

        Digest digest;
        for (std::size_t i = 0; i < 16; ++i)
            digest[i] = static_cast<std::uint8_t>(state[i / 4] >> (8 * (i % 4)));

        reset();
        return digest;
    }

    void Md5::transform(const std::uint8_t *block) noexcept
    {
        std::array<std::uint32_t, 16> words;
        for (std::size_t i = 0; i < 16; ++i)
        {
            words[i] = static_cast<std::uint32_t>(block[i * 4]) | static_cast<std::uint32_t>(block[i * 4 + 1]) << 8 |
                       static_cast<std::uint32_t>(block[i * 4 + 2]) << 16 |
                       static_cast<std::uint32_t>(block[i * 4 + 3]) << 24;
        }

        std::uint32_t a = state[0];
        std::uint32_t b = state[1];
        std::uint32_t c = state[2];
        std::uint32_t d = state[3];

        for (std::size_t i = 0; i < 64; ++i)
        {
            std::uint32_t f;
            std::size_t   g;
            if (i < 16)
            {
                f = (b & c) | (~b & d);
                g = i;
            }
            else if (i < 32)
            {
                f = (d & b) | (~d & c);
                g = (5 * i + 1) % 16;
            }
            else if (i < 48)
            {
                f = b ^ c ^ d;
                g = (3 * i + 5) % 16;
            }
            else
            {
                f = c ^ (b | ~d);
                g = (7 * i) % 16;
            }

            f = f + a + constants[i] + words[g];
            a = d;
            d = c;
            c = b;
            b = b + std::rotl(f, shifts[i]);
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
    }

    std::string Md5::toHex(const Digest &digest)
    {
        constexpr std::string_view digits = "0123456789abcdef";

        std::string result;
        result.reserve(digest.size() * 2);
        for (std::uint8_t byte : digest)
        {
            result.push_back(digits[byte >> 4]);
            result.push_back(digits[byte & 0x0f]);
        }
        return result;
    }

    std::string Md5::hex(std::string_view text)
    {
        Md5 md5;
        md5.update(text);
        return toHex(md5.finish());
    }
}
