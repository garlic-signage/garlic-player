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
#ifndef GARLIC_CORE_DOWNLOADER_DIGEST_HPP
#define GARLIC_CORE_DOWNLOADER_DIGEST_HPP

#include "downloader/http_transport.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace garlic::core::downloader
{
    /// The digest of a file as a server tells it: "sha256" or "md5" and the bytes of the hash.
    struct ExpectedDigest
    {
        std::string               algorithm;
        std::vector<std::uint8_t> value;
    };

    /**
     * The digest a server sends with a file, SHA-256 before MD5; nullopt if it sends none (then nothing is checked).
     * The headers, newest first:
     *   Repr-Digest    (RFC 9530)  sha-256=:<base64>:, md5=:<base64>:  the whole file, also in a 206 answer
     *   Content-Digest (RFC 9530)  the same syntax                     the bytes of this answer: not for a 206
     *   Digest         (RFC 3230)  SHA-256=<base64>, MD5=<base64>       the whole file
     *   Content-MD5    (RFC 1864)  <base64>                             the bytes of this answer: not for a 206
     * partial: the answer is a 206, it continues a .part file.
     */
    std::optional<ExpectedDigest> expectedDigest(const HttpHeaders &headers, bool partial);

    /// Standard base64 (RFC 4648, with padding or without); nullopt for another character.
    std::optional<std::vector<std::uint8_t>> decodeBase64(std::string_view text);
}

#endif // GARLIC_CORE_DOWNLOADER_DIGEST_HPP
