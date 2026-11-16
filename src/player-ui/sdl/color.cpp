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
#include "color.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <string>
#include <vector>

namespace player
{
    namespace
    {
        struct Named
        {
                std::string_view name;
                std::uint32_t    rgb;
        };

        // the CSS color names, sorted for the binary search
        constexpr Named names[] = {
            {"aliceblue", 0xf0f8ff},       {"antiquewhite", 0xfaebd7},    {"aqua", 0x00ffff},
            {"aquamarine", 0x7fffd4},      {"azure", 0xf0ffff},           {"beige", 0xf5f5dc},
            {"bisque", 0xffe4c4},          {"black", 0x000000},           {"blanchedalmond", 0xffebcd},
            {"blue", 0x0000ff},            {"blueviolet", 0x8a2be2},      {"brown", 0xa52a2a},
            {"burlywood", 0xdeb887},       {"cadetblue", 0x5f9ea0},       {"chartreuse", 0x7fff00},
            {"chocolate", 0xd2691e},       {"coral", 0xff7f50},           {"cornflowerblue", 0x6495ed},
            {"cornsilk", 0xfff8dc},        {"crimson", 0xdc143c},         {"cyan", 0x00ffff},
            {"darkblue", 0x00008b},        {"darkcyan", 0x008b8b},        {"darkgoldenrod", 0xb8860b},
            {"darkgray", 0xa9a9a9},        {"darkgreen", 0x006400},       {"darkgrey", 0xa9a9a9},
            {"darkkhaki", 0xbdb76b},       {"darkmagenta", 0x8b008b},     {"darkolivegreen", 0x556b2f},
            {"darkorange", 0xff8c00},      {"darkorchid", 0x9932cc},      {"darkred", 0x8b0000},
            {"darksalmon", 0xe9967a},      {"darkseagreen", 0x8fbc8f},    {"darkslateblue", 0x483d8b},
            {"darkslategray", 0x2f4f4f},   {"darkslategrey", 0x2f4f4f},   {"darkturquoise", 0x00ced1},
            {"darkviolet", 0x9400d3},      {"deeppink", 0xff1493},        {"deepskyblue", 0x00bfff},
            {"dimgray", 0x696969},         {"dimgrey", 0x696969},         {"dodgerblue", 0x1e90ff},
            {"firebrick", 0xb22222},       {"floralwhite", 0xfffaf0},     {"forestgreen", 0x228b22},
            {"fuchsia", 0xff00ff},         {"gainsboro", 0xdcdcdc},       {"ghostwhite", 0xf8f8ff},
            {"gold", 0xffd700},            {"goldenrod", 0xdaa520},       {"gray", 0x808080},
            {"green", 0x008000},           {"greenyellow", 0xadff2f},     {"grey", 0x808080},
            {"honeydew", 0xf0fff0},        {"hotpink", 0xff69b4},         {"indianred", 0xcd5c5c},
            {"indigo", 0x4b0082},          {"ivory", 0xfffff0},           {"khaki", 0xf0e68c},
            {"lavender", 0xe6e6fa},        {"lavenderblush", 0xfff0f5},   {"lawngreen", 0x7cfc00},
            {"lemonchiffon", 0xfffacd},    {"lightblue", 0xadd8e6},       {"lightcoral", 0xf08080},
            {"lightcyan", 0xe0ffff},       {"lightgoldenrodyellow", 0xfafad2}, {"lightgray", 0xd3d3d3},
            {"lightgreen", 0x90ee90},      {"lightgrey", 0xd3d3d3},       {"lightpink", 0xffb6c1},
            {"lightsalmon", 0xffa07a},     {"lightseagreen", 0x20b2aa},   {"lightskyblue", 0x87cefa},
            {"lightslategray", 0x778899},  {"lightslategrey", 0x778899},  {"lightsteelblue", 0xb0c4de},
            {"lightyellow", 0xffffe0},     {"lime", 0x00ff00},            {"limegreen", 0x32cd32},
            {"linen", 0xfaf0e6},           {"magenta", 0xff00ff},         {"maroon", 0x800000},
            {"mediumaquamarine", 0x66cdaa}, {"mediumblue", 0x0000cd},     {"mediumorchid", 0xba55d3},
            {"mediumpurple", 0x9370db},    {"mediumseagreen", 0x3cb371},  {"mediumslateblue", 0x7b68ee},
            {"mediumspringgreen", 0x00fa9a}, {"mediumturquoise", 0x48d1cc}, {"mediumvioletred", 0xc71585},
            {"midnightblue", 0x191970},    {"mintcream", 0xf5fffa},       {"mistyrose", 0xffe4e1},
            {"moccasin", 0xffe4b5},        {"navajowhite", 0xffdead},     {"navy", 0x000080},
            {"oldlace", 0xfdf5e6},         {"olive", 0x808000},           {"olivedrab", 0x6b8e23},
            {"orange", 0xffa500},          {"orangered", 0xff4500},       {"orchid", 0xda70d6},
            {"palegoldenrod", 0xeee8aa},   {"palegreen", 0x98fb98},       {"paleturquoise", 0xafeeee},
            {"palevioletred", 0xdb7093},   {"papayawhip", 0xffefd5},      {"peachpuff", 0xffdab9},
            {"peru", 0xcd853f},            {"pink", 0xffc0cb},            {"plum", 0xdda0dd},
            {"powderblue", 0xb0e0e6},      {"purple", 0x800080},          {"rebeccapurple", 0x663399},
            {"red", 0xff0000},             {"rosybrown", 0xbc8f8f},       {"royalblue", 0x4169e1},
            {"saddlebrown", 0x8b4513},     {"salmon", 0xfa8072},          {"sandybrown", 0xf4a460},
            {"seagreen", 0x2e8b57},        {"seashell", 0xfff5ee},        {"sienna", 0xa0522d},
            {"silver", 0xc0c0c0},          {"skyblue", 0x87ceeb},         {"slateblue", 0x6a5acd},
            {"slategray", 0x708090},       {"slategrey", 0x708090},       {"snow", 0xfffafa},
            {"springgreen", 0x00ff7f},     {"steelblue", 0x4682b4},       {"tan", 0xd2b48c},
            {"teal", 0x008080},            {"thistle", 0xd8bfd8},         {"tomato", 0xff6347},
            {"turquoise", 0x40e0d0},       {"violet", 0xee82ee},          {"wheat", 0xf5deb3},
            {"white", 0xffffff},           {"whitesmoke", 0xf5f5f5},      {"yellow", 0xffff00},
            {"yellowgreen", 0x9acd32},
        };

        ui::Color fromRgb(std::uint32_t rgb, unsigned char alpha = 255)
        {
            return {static_cast<unsigned char>(rgb >> 16U), static_cast<unsigned char>(rgb >> 8U),
                    static_cast<unsigned char>(rgb), alpha};
        }

        std::optional<std::uint32_t> hex(std::string_view digits)
        {
            std::uint32_t value = 0;
            auto [end, error]   = std::from_chars(digits.data(), digits.data() + digits.size(), value, 16);
            if (error != std::errc{} || end != digits.data() + digits.size())
                return std::nullopt;
            return value;
        }

        /// "rgb(1, 2, 3)" → {"1", "2", "3"}
        std::vector<std::string> arguments(std::string_view text)
        {
            std::vector<std::string> parts(1);
            for (char c : text)
            {
                if (c == ',')
                    parts.emplace_back();
                else if (std::isspace(static_cast<unsigned char>(c)) == 0)
                    parts.back() += c;
            }
            return parts;
        }

        std::optional<double> number(const std::string &text)
        {
            double value      = 0;
            auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || text.empty())
                return std::nullopt;
            if (end == text.data() + text.size() - 1 && *end == '%')
                return value * 2.55; // a percentage of 255
            if (end != text.data() + text.size())
                return std::nullopt;
            return value;
        }

        unsigned char channel(double value)
        {
            return static_cast<unsigned char>(std::clamp(value, 0.0, 255.0) + 0.5);
        }
    }

    std::optional<ui::Color> parseColor(std::string_view raw)
    {
        std::string text;
        for (char c : raw)
            if (std::isspace(static_cast<unsigned char>(c)) == 0 || !text.empty())
                text += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
            text.pop_back();

        if (text == "transparent")
            return ui::Color{0, 0, 0, 0};

        if (text.starts_with('#'))
        {
            std::string_view digits = std::string_view(text).substr(1);
            auto             value  = hex(digits);
            if (!value)
                return std::nullopt;
            if (digits.size() == 3) // #rgb: every digit twice
            {
                std::uint32_t r = (*value >> 8U) & 0xFU, g = (*value >> 4U) & 0xFU, b = *value & 0xFU;
                return fromRgb((r * 17U << 16U) | (g * 17U << 8U) | (b * 17U));
            }
            if (digits.size() == 6)
                return fromRgb(*value);
            if (digits.size() == 8) // #aarrggbb like QColor
                return fromRgb(*value & 0xFFFFFFU, static_cast<unsigned char>(*value >> 24U));
            return std::nullopt;
        }

        bool with_alpha = text.starts_with("rgba(");
        if ((with_alpha || text.starts_with("rgb(")) && text.ends_with(')'))
        {
            std::size_t open  = text.find('(');
            auto        parts = arguments(std::string_view(text).substr(open + 1, text.size() - open - 2));
            if (parts.size() != (with_alpha ? 4U : 3U))
                return std::nullopt;
            auto r = number(parts[0]), g = number(parts[1]), b = number(parts[2]);
            if (!r || !g || !b)
                return std::nullopt;
            unsigned char alpha = 255;
            if (with_alpha)
            {
                auto a = number(parts[3]);
                if (!a)
                    return std::nullopt;
                alpha = channel(*a <= 1.0 ? *a * 255 : *a);
            }
            return ui::Color{channel(*r), channel(*g), channel(*b), alpha};
        }

        auto found = std::ranges::lower_bound(names, std::string_view(text), {}, &Named::name);
        if (found != std::ranges::end(names) && found->name == text)
            return fromRgb(found->rgb);
        return std::nullopt;
    }
}
