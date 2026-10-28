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
#ifndef GARLIC_CORE_TESTS_SMIL_HELPERS_HPP
#define GARLIC_CORE_TESTS_SMIL_HELPERS_HPP

#include <doctest/doctest.h>

#include "smil/parser.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace garlic::core::tests
{
    using namespace garlic::core::smil;

    inline std::string readFile(const std::filesystem::path &path)
    {
        std::ifstream      in(path, std::ios::binary);
        std::ostringstream content;
        content << in.rdbuf();
        return content.str();
    }

    inline Document parseFile(const std::string &name)
    {
        auto document = parse(readFile(std::filesystem::path(GARLIC_CORE_TEST_DATA) / name));
        REQUIRE(document.has_value());
        return *document;
    }

    inline Container parseBody(std::string_view body)
    {
        auto document = parse("<smil><body>" + std::string(body) + "</body></smil>");
        REQUIRE(document.has_value());
        return document->body;
    }

    /// The tree as text: containers as "id(children)", media as "id".
    inline std::string outline(const Node &node);

    inline std::string outline(const Container &container)
    {
        std::string text = container.id + "(";
        for (std::size_t i = 0; i < container.children.size(); ++i)
            text += (i > 0 ? " " : "") + outline(container.children[i]);
        return text + ")";
    }

    inline std::string outline(const Node &node)
    {
        if (const auto *media = std::get_if<Media>(&node))
            return media->id;
        return outline(std::get<Container>(node));
    }

    inline const Container &container(const Node &node)
    {
        REQUIRE(std::holds_alternative<Container>(node));
        return std::get<Container>(node);
    }

    inline std::chrono::milliseconds offset(const TimeCondition &condition)
    {
        REQUIRE(std::holds_alternative<Offset>(condition));
        return std::get<Offset>(condition).value;
    }
}

#endif // GARLIC_CORE_TESTS_SMIL_HELPERS_HPP
