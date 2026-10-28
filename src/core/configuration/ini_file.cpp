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
#include "configuration/ini_file.hpp"

#include <charconv>
#include <cstdint>
#include <vector>

namespace garlic::core::configuration
{
    namespace
    {
        constexpr std::string_view general = "General";

        std::string_view trim(std::string_view text)
        {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
                text.remove_prefix(1);
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'))
                text.remove_suffix(1);
            return text;
        }

        bool isHex(char c)
        {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        }

        void appendUtf8(std::string &out, std::uint32_t cp)
        {
            if (cp < 0x80)
                out.push_back(static_cast<char>(cp));
            else if (cp < 0x800)
            {
                out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            else if (cp < 0x10000)
            {
                out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
        }

        std::uint32_t parseHex(std::string_view digits)
        {
            std::uint32_t value = 0;
            std::from_chars(digits.data(), digits.data() + digits.size(), value, 16);
            return value;
        }

        /// QSettings writes special characters in keys as %XX or %UXXXX.
        std::string unescapeKey(std::string_view key)
        {
            auto hexAt = [&](std::size_t pos, std::size_t count)
            {
                if (pos + count > key.size())
                    return false;
                for (std::size_t i = pos; i < pos + count; ++i)
                {
                    if (!isHex(key[i]))
                        return false;
                }
                return true;
            };

            std::string out;
            for (std::size_t i = 0; i < key.size(); ++i)
            {
                if (key[i] == '%' && i + 1 < key.size() && key[i + 1] == 'U' && hexAt(i + 2, 4))
                {
                    appendUtf8(out, parseHex(key.substr(i + 2, 4)));
                    i += 5;
                }
                else if (key[i] == '%' && hexAt(i + 1, 2))
                {
                    appendUtf8(out, parseHex(key.substr(i + 1, 2)));
                    i += 2;
                }
                else if (key[i] == '\\')
                    out.push_back('/'); // "b\\c" in [a] is the key "a/b/c"
                else
                    out.push_back(key[i]);
            }
            return out;
        }

        std::string escapeKey(std::string_view key)
        {
            constexpr std::string_view digits = "0123456789ABCDEF";

            std::string out;
            for (char c : key)
            {
                auto u = static_cast<unsigned char>(c);
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                    c == '-' || c == '.' || u >= 0x80)
                    out.push_back(c);
                else if (c == '/')
                    out.push_back('\\');
                else
                {
                    out.push_back('%');
                    out.push_back(digits[u >> 4]);
                    out.push_back(digits[u & 0x0F]);
                }
            }
            return out;
        }

        /// Value as QSettings writes it: quotes, backslash escapes, ';' starts a comment outside quotes.
        std::string unescapeValue(std::string_view value)
        {
            if (value.starts_with("@@"))
                value.remove_prefix(1); // "@@x" is the string "@x"

            std::string out;
            std::size_t kept   = 0; // length up to the last quoted or escaped character, never trimmed
            bool        quoted = false;
            for (std::size_t i = 0; i < value.size(); ++i)
            {
                char c = value[i];
                if (c == '"')
                    quoted = !quoted;
                else if (c == ';' && !quoted)
                    break;
                else if (c == '\\' && i + 1 < value.size())
                {
                    char next = value[++i];
                    switch (next)
                    {
                        case 'a': out.push_back('\a'); break;
                        case 'b': out.push_back('\b'); break;
                        case 'f': out.push_back('\f'); break;
                        case 'n': out.push_back('\n'); break;
                        case 'r': out.push_back('\r'); break;
                        case 't': out.push_back('\t'); break;
                        case 'v': out.push_back('\v'); break;
                        case 'x':
                        {
                            std::size_t start = i + 1;
                            std::size_t end   = start;
                            while (end < value.size() && isHex(value[end])) // like QSettings: all hex digits
                                ++end;
                            appendUtf8(out, parseHex(value.substr(start, end - start)));
                            i = end - 1;
                            break;
                        }
                        default: out.push_back(next); break; // \\ \" \; …
                    }
                    kept = out.size();
                }
                else
                {
                    out.push_back(c);
                    if (quoted)
                        kept = out.size();
                }
            }

            // spaces before a comment ("value ; comment") are not part of the value
            while (out.size() > kept && out.back() == ' ')
                out.pop_back();
            return out;
        }

        std::string escapeValue(std::string_view value)
        {
            bool needs_quotes = !value.empty() && (value.front() == ' ' || value.back() == ' ');

            std::string out;
            if (value.starts_with('@'))
                out.push_back('@'); // QSettings reads "@…" as a typed value

            constexpr std::string_view digits = "0123456789abcdef";
            bool after_hex_escape = false; // a hex digit right after \xHH would be read as part of it
            for (char c : value)
            {
                bool hex_escape = false;
                switch (c)
                {
                    case '\\': out += "\\\\"; break;
                    case '"':  out += "\\\""; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    case ',':
                    case ';':
                    case '=':
                        needs_quotes = true;
                        out.push_back(c);
                        break;
                    default:
                        if (static_cast<unsigned char>(c) < 0x20 || (after_hex_escape && isHex(c)))
                        {
                            out += "\\x";
                            out.push_back(digits[static_cast<unsigned char>(c) >> 4]);
                            out.push_back(digits[c & 0x0F]);
                            hex_escape = true;
                        }
                        else
                            out.push_back(c);
                }
                after_hex_escape = hex_escape;
            }
            return needs_quotes ? "\"" + out + "\"" : out;
        }

        /// "httpd/port" → {"httpd", "port"}, "uuid" → {"", "uuid"}
        std::pair<std::string, std::string> splitKey(std::string_view key)
        {
            auto slash = key.find('/');
            if (slash == std::string_view::npos)
                return {std::string{}, std::string(key)};
            return {std::string(key.substr(0, slash)), std::string(key.substr(slash + 1))};
        }
    }

    IniFile IniFile::parse(std::string_view text)
    {
        IniFile     ini;
        std::string section;

        while (!text.empty())
        {
            auto             end  = text.find('\n');
            std::string_view line = trim(text.substr(0, end));
            text                  = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);

            if (line.empty() || line.front() == ';' || line.front() == '#')
                continue;

            if (line.front() == '[' && line.back() == ']')
            {
                section = unescapeKey(line.substr(1, line.size() - 2));
                if (section == general)
                    section.clear();
                else if (section == "%General")
                    section = general; // a real group named "General"
                continue;
            }

            auto equal = line.find('=');
            if (equal == std::string_view::npos)
                continue;

            std::string key = unescapeKey(trim(line.substr(0, equal)));
            if (!section.empty())
                key = section + "/" + key;
            ini.entries.insert_or_assign(std::move(key), unescapeValue(trim(line.substr(equal + 1))));
        }
        return ini;
    }

    std::string IniFile::serialize() const
    {
        // keys without group first as [General], then the groups in order; entries are sorted by full key
        std::map<std::string, std::vector<std::pair<std::string, std::string>>> groups;
        for (const auto &[key, value] : entries)
        {
            auto [group, name] = splitKey(key);
            groups[group].emplace_back(name, value);
        }

        std::string out;
        for (const auto &[group, lines] : groups)
        {
            if (!out.empty())
                out += "\n";

            if (group.empty())
                out += "[General]\n";
            else if (group == general)
                out += "[%General]\n"; // a real group named "General"
            else
                out += "[" + escapeKey(group) + "]\n";

            for (const auto &[name, value] : lines)
                out += escapeKey(name) + "=" + escapeValue(value) + "\n";
        }
        return out;
    }

    std::optional<std::string> IniFile::value(std::string_view key) const
    {
        auto it = entries.find(key);
        if (it == entries.end())
            return std::nullopt;
        return it->second;
    }

    void IniFile::setValue(std::string key, std::string value)
    {
        entries.insert_or_assign(std::move(key), std::move(value));
    }

    void IniFile::remove(std::string_view key)
    {
        if (auto it = entries.find(key); it != entries.end())
            entries.erase(it);
    }
}
