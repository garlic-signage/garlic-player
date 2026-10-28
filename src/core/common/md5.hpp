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
#ifndef GARLIC_CORE_COMMON_MD5_HPP
#define GARLIC_CORE_COMMON_MD5_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace garlic::core::common
{
    /**
     * MD5 (RFC 1321) for cache file names and file comparison. Not for security purposes.
     *
     * Own implementation, because OpenSSL is not available on WASM and ESP32. Data can be added in chunks, so large
     * files never have to be in RAM completely.
     */
    class Md5
    {
        public:
            using Digest = std::array<std::uint8_t, 16>;

            Md5() noexcept;

            void update(std::span<const std::byte> data) noexcept;
            void update(std::string_view text) noexcept;

            /// Finishes the calculation. Afterwards the object starts again from the beginning.
            Digest finish() noexcept;

            /// Lowercase hex string of a digest, e.g. "d41d8cd98f00b204e9800998ecf8427e".
            static std::string toHex(const Digest &digest);

            /// MD5 of a string as lowercase hex string.
            static std::string hex(std::string_view text);

        private:
            void reset() noexcept;
            void transform(const std::uint8_t *block) noexcept;

            std::array<std::uint32_t, 4> state;
            std::array<std::uint8_t, 64> buffer;
            std::size_t                  buffered = 0;
            std::uint64_t                length   = 0; // in bytes
    };
}

#endif // GARLIC_CORE_COMMON_MD5_HPP
