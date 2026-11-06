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

/**
 * Golden files: every *.smil under the test data runs for a fixed time, its timeline must equal the *.expected file
 * at the same path under data/golden (one entry per line). A new SMIL without *.expected fails, so every file in the
 * directory is part of the release check.
 *
 * GARLIC_UPDATE_GOLDEN=1 writes the *.expected files from the current behavior instead of comparing. Review the diff
 * before committing: a golden file is only as right as the behavior it was written from.
 */
#include "engine_runner.hpp"
#include "time_zone.hpp"

#include <algorithm>
#include <cstdlib>
#include <ranges>

using namespace garlic::core::smil;
using namespace garlic::core::tests;
using namespace std::chrono_literals;

namespace
{
    constexpr auto run_time     = 120s; // simulated time per file
    constexpr auto media_length = 10s;  // every video and audio "plays" this long

    void setMediaLength(EngineRunner &runner, const Node &node)
    {
        if (const auto *media = std::get_if<Media>(&node))
        {
            if (media->kind == MediaKind::Video || media->kind == MediaKind::Audio)
                runner.media_length[media->src] = media_length;
            return;
        }
        for (const Node &child : std::get<Container>(node).children)
            setMediaLength(runner, child);
    }

    /// The timeline of the file, one entry per line, or the parse error.
    std::string timeline(const std::filesystem::path &file)
    {
        std::string xml    = readFile(file);
        auto        parsed = parse(xml);
        if (!parsed)
            return std::format("parse error, line {}: {}\n", parsed.error().line, parsed.error().message);

        EngineRunner runner(xml);
        setMediaLength(runner, runner.document.body);

        std::string result;
        for (auto entry : std::views::split(runner.run(run_time), std::string_view(" | ")))
            result += std::string(entry.begin(), entry.end()) + "\n";
        return result;
    }

    std::vector<std::filesystem::path> smilFiles()
    {
        std::vector<std::filesystem::path> files;
        for (const auto &entry : std::filesystem::recursive_directory_iterator(GARLIC_CORE_TEST_DATA))
            if (entry.is_regular_file() && entry.path().extension() == ".smil")
                files.push_back(entry.path());
        std::ranges::sort(files);
        return files;
    }
}

TEST_CASE("golden files: every SMIL of the test data plays as in its *.expected")
{
    TimeZone   tz("UTC");
    const bool update = std::getenv("GARLIC_UPDATE_GOLDEN") != nullptr;
    auto       files  = smilFiles();
    REQUIRE_FALSE(files.empty());

    for (const auto &file : files)
    {
        auto name     = std::filesystem::relative(file, GARLIC_CORE_TEST_DATA).generic_string();
        auto expected = (std::filesystem::path(GARLIC_CORE_TEST_GOLDEN) / name).replace_extension(".expected");
        auto actual   = timeline(file);
        INFO(name);

        if (update)
        {
            std::filesystem::create_directories(expected.parent_path());
            std::ofstream(expected, std::ios::binary) << actual;
            continue;
        }
        if (!std::filesystem::exists(expected))
        {
            FAIL_CHECK("no golden file, run with GARLIC_UPDATE_GOLDEN=1 and review it");
            continue;
        }
        CHECK(readFile(expected) == actual);
    }
}
