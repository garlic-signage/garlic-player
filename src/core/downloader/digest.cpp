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
#include "downloader/digest.hpp"

#include "common/text.hpp"


namespace garlic::core::downloader
{
    namespace
    {
        std::string_view trim(std::string_view text)
        {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
                text.remove_prefix(1);
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
                text.remove_suffix(1);
            return text;
        }

        /// "sha-256" / "SHA-256" → "sha256", "md5" / "MD5" → "md5"; others empty (sha-512 and the like: not checked)
        std::string algorithmName(std::string_view name)
        {
            std::string lower = common::toLower(trim(name));
            if (lower == "sha-256")
                return "sha256";
            if (lower == "md5")
                return "md5";
            return {};
        }

        bool fits(const ExpectedDigest &digest)
        {
            return digest.value.size() == (digest.algorithm == "sha256" ? 32U : 16U);
        }

        /// The entries of a list "a=x, b=y"; structured (RFC 9530): the value between colons
        std::optional<ExpectedDigest> pick(std::string_view list, bool structured)
        {
            std::optional<ExpectedDigest> md5;
            while (!list.empty())
            {
                auto        comma = list.find(',');
                std::string_view entry = trim(list.substr(0, comma));
                list = comma == std::string_view::npos ? std::string_view{} : list.substr(comma + 1);

                auto equals = entry.find('=');
                if (equals == std::string_view::npos)
                    continue;
                std::string      algorithm = algorithmName(entry.substr(0, equals));
                std::string_view value     = trim(entry.substr(equals + 1));
                if (structured)
                {
                    if (value.size() < 2 || value.front() != ':' || value.back() != ':')
                        continue;
                    value = value.substr(1, value.size() - 2);
                }
                auto bytes = decodeBase64(value);
                if (algorithm.empty() || !bytes)
                    continue;
                ExpectedDigest digest{algorithm, std::move(*bytes)};
                if (!fits(digest))
                    continue;
                if (algorithm == "sha256")
                    return digest;
                md5 = std::move(digest);
            }
            return md5;
        }
    }

    std::optional<ExpectedDigest> expectedDigest(const HttpHeaders &headers, bool partial)
    {
        std::optional<ExpectedDigest> found;
        auto better = [&found](std::optional<ExpectedDigest> next)
        {
            if (next && (!found || (found->algorithm == "md5" && next->algorithm == "sha256")))
                found = std::move(next);
        };
        if (auto value = headers.find("Repr-Digest"))
            better(pick(*value, true));
        if (!partial)
            if (auto value = headers.find("Content-Digest"))
                better(pick(*value, true));
        if (auto value = headers.find("Digest"))
            better(pick(*value, false));
        if (!partial)
            if (auto value = headers.find("Content-MD5"))
                if (auto bytes = decodeBase64(trim(*value)))
                    better(ExpectedDigest{"md5", std::move(*bytes)});
        if (found && !fits(*found))
            return std::nullopt;
        return found;
    }

    std::optional<std::vector<std::uint8_t>> decodeBase64(std::string_view text)
    {
        constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        while (!text.empty() && text.back() == '=')
            text.remove_suffix(1);
        std::vector<std::uint8_t> bytes;
        std::uint32_t             bits  = 0;
        int                       count = 0;
        for (char c : text)
        {
            auto index = alphabet.find(c);
            if (index == std::string_view::npos)
                return std::nullopt;
            bits = bits << 6 | static_cast<std::uint32_t>(index);
            count += 6;
            if (count >= 8)
            {
                count -= 8;
                bytes.push_back(static_cast<std::uint8_t>(bits >> count));
                bits &= (1U << count) - 1;
            }
        }
        return bytes;
    }
}
