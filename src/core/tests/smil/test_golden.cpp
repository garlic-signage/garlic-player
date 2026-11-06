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
 * A file runs from 2026-01-01 00:00 UTC (a Thursday) for 120 s. Files whose wallclock times lie elsewhere get their
 * own start and run time in windows; the first line of the *.expected shows both.
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
    using namespace std::chrono;

    constexpr auto media_length = 10s; // every video and audio "plays" this long

    struct Window
    {
        std::string_view file;
        sys_seconds      start    = sys_days{2026y / January / 1};
        seconds          run_time = 120s;
    };

    /// Start and run time of the files whose wallclock times are not at the default start.
    const std::vector<Window> windows = {
        // the one-time trigger at 00:45:15 between the ones every 30 s
        {"excl/bugs/2_parallel_trigger_past.smil", sys_days{2022y / July / 8} + 44min + 50s, 60s},
        // all dated wallclocks from 01:05:00 to 01:12:20
        {"excl/bugs/3_hadi.smil", sys_days{2022y / July / 8} + 1h + 4min + 50s, 8min},
        // one whole week from Monday 00:00, every priorityClass at least once
        {"excl/complex/1_customer.smil", sys_days{2026y / January / 5}, days{7}},
        // Monday 23:59:59 to Tuesday 00:00
        {"excl/complex/2_signapp.smil", sys_days{2026y / January / 5} + 23h + 59min, 120s},
        // end of the day at 23:59:59 and begin again at 00:00, with the triggers every 10 s
        {"excl/trigger/6_priority_realworld.smil", sys_days{2026y / January / 1} + 23h + 59min, 120s},
        {"par/trigger/3_changes.smil", sys_days{2026y / January / 1} + 23h + 59min, 120s},
        // seq2 begins at 07:00 (period P0H)
        {"issues/31.smil", sys_days{2022y / March / 29} + 6h + 59min + 30s, 60s},
        // 23:30 to 23:40 one image per minute
        {"issues/33.smil", sys_days{2022y / March / 29} + 23h + 29min + 30s, 11min},
    };

    Window window(std::string_view name)
    {
        for (const Window &w : windows)
            if (w.file == name)
                return w;
        return {name};
    }

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
    std::string timeline(const std::filesystem::path &file, const Window &w)
    {
        std::string result = std::format("from {:%F %T} for {}\n", w.start, w.run_time);
        std::string xml    = readFile(file);
        auto        parsed = parse(xml);
        if (!parsed)
            return result + std::format("parse error, line {}: {}\n", parsed.error().line, parsed.error().message);

        EngineRunner runner(xml);
        setMediaLength(runner, runner.document.body);
        runner.clock.setWallTime(w.start);

        for (auto entry : std::views::split(runner.run(w.run_time), std::string_view(" | ")))
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
    for (const Window &w : windows)
        CHECK_MESSAGE(std::filesystem::exists(std::filesystem::path(GARLIC_CORE_TEST_DATA) / w.file), w.file);

    for (const auto &file : files)
    {
        auto name     = std::filesystem::relative(file, GARLIC_CORE_TEST_DATA).generic_string();
        auto expected = (std::filesystem::path(GARLIC_CORE_TEST_GOLDEN) / name).replace_extension(".expected");
        auto actual   = timeline(file, window(name));
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
