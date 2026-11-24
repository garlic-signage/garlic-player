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
#include "common/sha256.hpp"

#include "common/text.hpp"

namespace garlic::core::common
{
    namespace
    {
        /// Like rotateLeft of md5.cpp: no std::rotr, its count type differs between the standard and NDK 26. 0 < n < 32.
        constexpr std::uint32_t rotateRight(std::uint32_t value, unsigned count)
        {
            return (value >> count) | (value << (32U - count));
        }

        // The first 32 bits of the fractional parts of the cube roots of the first 64 primes (FIPS 180-4, 4.2.2)
        constexpr std::array<std::uint32_t, 64> constants = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    }

    Sha256::Sha256() noexcept
    {
        reset();
    }

    void Sha256::reset() noexcept
    {
        state    = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        input.reset();
    }

    void Sha256::update(std::span<const std::byte> data) noexcept
    {
        input.add(data, [this](const std::uint8_t *full) { transform(full); });
    }

    void Sha256::update(std::string_view text) noexcept
    {
        update(std::as_bytes(std::span(text.data(), text.size())));
    }

    Sha256::Digest Sha256::finish() noexcept
    {
        input.pad(true, [this](const std::uint8_t *full) { transform(full); });

        Digest digest{};
        for (std::size_t i = 0; i < state.size(); ++i)
            for (std::size_t b = 0; b < 4; ++b) // state big-endian
                digest[i * 4 + b] = static_cast<std::uint8_t>(state[i] >> ((3 - b) * 8));
        reset();
        return digest;
    }

    void Sha256::transform(const std::uint8_t *block) noexcept
    {
        std::array<std::uint32_t, 64> w{};
        for (std::size_t i = 0; i < 16; ++i)
            w[i] = static_cast<std::uint32_t>(block[i * 4]) << 24 | static_cast<std::uint32_t>(block[i * 4 + 1]) << 16 |
                   static_cast<std::uint32_t>(block[i * 4 + 2]) << 8 | static_cast<std::uint32_t>(block[i * 4 + 3]);
        for (std::size_t i = 16; i < 64; ++i)
        {
            std::uint32_t s0 = rotateRight(w[i - 15], 7) ^ rotateRight(w[i - 15], 18) ^ (w[i - 15] >> 3);
            std::uint32_t s1 = rotateRight(w[i - 2], 17) ^ rotateRight(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i]             = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        std::uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
        for (std::size_t i = 0; i < 64; ++i)
        {
            std::uint32_t s1    = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
            std::uint32_t ch    = (e & f) ^ (~e & g);
            std::uint32_t temp1 = h + s1 + ch + constants[i] + w[i];
            std::uint32_t s0    = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
            std::uint32_t maj   = (a & b) ^ (a & c) ^ (b & c);
            std::uint32_t temp2 = s0 + maj;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    std::string Sha256::toHex(const Digest &digest)
    {
        return common::toHex(digest);
    }

    std::string Sha256::hex(std::string_view text)
    {
        Sha256 sha;
        sha.update(text);
        return toHex(sha.finish());
    }
}
