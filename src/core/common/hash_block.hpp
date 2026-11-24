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
#ifndef GARLIC_CORE_COMMON_HASH_BLOCK_HPP
#define GARLIC_CORE_COMMON_HASH_BLOCK_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace garlic::core::common
{
    /**
     * The 64 byte block buffer that Md5 and Sha256 share: collects the data and hands every full block to the
     * transform of the hash. pad() appends 0x80, zeros and the length in bits, as both algorithms do; they differ
     * only in the byte order of the length.
     */
    class HashBlock
    {
        public:
            void reset() noexcept
            {
                buffer   = {};
                buffered = 0;
                length   = 0;
            }

            template <typename Transform> void add(std::span<const std::byte> data, Transform transform) noexcept
            {
                length += data.size();
                for (std::byte byte : data)
                {
                    buffer[buffered++] = static_cast<std::uint8_t>(byte);
                    if (buffered == buffer.size())
                    {
                        transform(buffer.data());
                        buffered = 0;
                    }
                }
            }

            template <typename Transform> void pad(bool lengthBigEndian, Transform transform) noexcept
            {
                // Padding: 0x80, zeros until 56 bytes in the block, then the length in bits
                const std::uint64_t bits = length * 8;
                buffer[buffered++] = 0x80;
                if (buffered > 56)
                {
                    while (buffered < 64)
                        buffer[buffered++] = 0;
                    transform(buffer.data());
                    buffered = 0;
                }
                while (buffered < 56)
                    buffer[buffered++] = 0;
                for (std::size_t i = 0; i < 8; ++i)
                {
                    const std::size_t shift = lengthBigEndian ? (7 - i) * 8 : i * 8;
                    buffer[buffered++] = static_cast<std::uint8_t>(bits >> shift);
                }
                transform(buffer.data());
            }

        private:
            std::array<std::uint8_t, 64> buffer{};
            std::size_t                  buffered = 0;
            std::uint64_t                length   = 0; // in bytes
    };
}

#endif // GARLIC_CORE_COMMON_HASH_BLOCK_HPP
