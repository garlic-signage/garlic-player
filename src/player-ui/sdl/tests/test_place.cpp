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
#include "place.hpp"

#include <doctest/doctest.h>

using player::place;

namespace
{
    const SDL_FRect area{100, 50, 400, 200}; // a region of 400 × 200 at 100, 50

    bool same(const SDL_FRect &a, const SDL_FRect &b)
    {
        return a.x == doctest::Approx(b.x) && a.y == doctest::Approx(b.y) && a.w == doctest::Approx(b.w) &&
               a.h == doctest::Approx(b.h);
    }
}

TEST_CASE("fit: fill stretches, meet keeps the aspect inside, slice covers, without fit the original size")
{
    CHECK(same(place(area, 100, 100, "fill", "topleft"), {100, 50, 400, 200}));
    CHECK(same(place(area, 100, 100, "meet", "topleft"), {100, 50, 200, 200}));
    CHECK(same(place(area, 100, 100, "slice", "topleft"), {100, 50, 400, 400}));
    CHECK(same(place(area, 100, 100, "", "topleft"), {100, 50, 100, 100}));
    CHECK(same(place(area, 100, 100, "meetbest", "topleft"), {100, 50, 100, 100})); // smaller: never enlarged
    CHECK(same(place(area, 800, 800, "meetbest", "topleft"), {100, 50, 200, 200})); // larger: like meet
}

TEST_CASE("mediaAlign: the 9 positions, topLeft if unknown (garlic-lib)")
{
    // 100 × 100 in 400 × 200: 300 free across, 100 free down
    CHECK(same(place(area, 100, 100, "", "topleft"), {100, 50, 100, 100}));
    CHECK(same(place(area, 100, 100, "", "topmid"), {250, 50, 100, 100}));
    CHECK(same(place(area, 100, 100, "", "topright"), {400, 50, 100, 100}));
    CHECK(same(place(area, 100, 100, "", "midleft"), {100, 100, 100, 100}));
    CHECK(same(place(area, 100, 100, "", "center"), {250, 100, 100, 100}));
    CHECK(same(place(area, 100, 100, "", "midright"), {400, 100, 100, 100}));
    CHECK(same(place(area, 100, 100, "", "bottomleft"), {100, 150, 100, 100}));
    CHECK(same(place(area, 100, 100, "", "bottommid"), {250, 150, 100, 100}));
    CHECK(same(place(area, 100, 100, "", "bottomright"), {400, 150, 100, 100}));
    CHECK(same(place(area, 100, 100, "", "nonsense"), {100, 50, 100, 100}));
}

TEST_CASE("video like a 16:9 film: meet with mediaAlign puts it to that side, slice is cut on the other")
{
    // 1920 × 1080 in 400 × 200: meet is 355.6 × 200, 44.4 free across
    CHECK(same(place(area, 1920, 1080, "meet", "midleft"), {100, 50, 355.5556F, 200}));
    CHECK(same(place(area, 1920, 1080, "meet", "midright"), {144.4444F, 50, 355.5556F, 200}));
    // slice is 400 × 225: 25 too high, bottomMid cuts it at the top
    CHECK(same(place(area, 1920, 1080, "slice", "bottommid"), {100, 25, 400, 225}));
}
