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
#include "helpers.hpp"
#include "time_zone.hpp"

using namespace garlic::core::smil;
using namespace garlic::core::tests;
using namespace std::chrono;

namespace
{
    /// begin list of an image in a par (lists and wallclocks allowed)
    std::vector<TimeCondition> begin(std::string_view value)
    {
        Container body = parseBody(R"(<par><img src="a.jpg" begin=")" + std::string(value) + R"("/></par>)");
        return std::get<Media>(container(body.children.at(0)).children.at(0)).timing.begin;
    }

    template <typename T> T one(std::string_view value)
    {
        auto conditions = begin(value);
        REQUIRE(conditions.size() == 1);
        REQUIRE(std::holds_alternative<T>(conditions[0]));
        return std::get<T>(conditions[0]);
    }

    Wallclock wallclock(std::string_view value)
    {
        return one<Wallclock>("wallclock(" + std::string(value) + ")");
    }

    bool isDefault(std::string_view value)
    {
        auto conditions = begin(value);
        return conditions.size() == 1 && std::holds_alternative<Offset>(conditions[0]) &&
               std::get<Offset>(conditions[0]).value == 0ms;
    }
}

TEST_CASE("syncbase: id.begin and id.end with offset")
{
    auto a = one<SyncBase>("img1.begin");
    CHECK(a.id == "img1");
    CHECK(a.edge == SyncBase::Edge::Begin);
    CHECK(a.offset == 0ms);

    auto b = one<SyncBase>("seq1.end+2.5s");
    CHECK(b.id == "seq1");
    CHECK(b.edge == SyncBase::Edge::End);
    CHECK(b.offset == 2500ms);

    auto c = one<SyncBase>("my-img.v2.end-1s"); // garlic-lib cut the id at the first "-"
    CHECK(c.id == "my-img.v2");
    CHECK(c.offset == -1s);
}

TEST_CASE("events: beginEvent, endEvent, activateEvent, click")
{
    CHECK(one<Event>("img1.beginEvent").type == Event::Type::BeginEvent);
    CHECK(one<Event>("img1.endEvent").type == Event::Type::EndEvent);
    CHECK(one<Event>("img1.click").type == Event::Type::Click);

    auto activate = one<Event>("button.activateEvent+1s");
    CHECK(activate.id == "button");
    CHECK(activate.type == Event::Type::ActivateEvent);
    CHECK(activate.offset == 1s);
}

TEST_CASE("accesskey and notify")
{
    auto key = one<AccessKey>("accesskey(A)+2s");
    CHECK(key.key == 'a');
    CHECK(key.offset == 2s);
    CHECK(one<AccessKey>("accesskey(7)").key == '7');

    auto notify = one<Notify>("notify(door-open)-500ms"); // garlic-lib cut the name at "-"
    CHECK(notify.name == "door-open");
    CHECK(notify.offset == -500ms);
}

TEST_CASE("unreadable trigger values give the default begin")
{
    CHECK(isDefault("img1.middle"));
    CHECK(isDefault("img1.mouseover"));
    CHECK(isDefault(".begin"));
    CHECK(isDefault("img1.begin*2"));
    CHECK(isDefault("accesskey(ab)"));
    CHECK(isDefault("accesskey(#)"));
    CHECK(isDefault("accesskey(a"));
    CHECK(isDefault("notify()"));
    CHECK(isDefault("garbage"));
}

TEST_CASE("lists of different triggers")
{
    auto conditions = begin("0s; accesskey(a); notify(x); img1.end; wallclock(2026-01-01T08:00); broken.thing");
    REQUIRE(conditions.size() == 5);
    CHECK(std::holds_alternative<Offset>(conditions[0]));
    CHECK(std::holds_alternative<AccessKey>(conditions[1]));
    CHECK(std::holds_alternative<Notify>(conditions[2]));
    CHECK(std::holds_alternative<SyncBase>(conditions[3]));
    CHECK(std::holds_alternative<Wallclock>(conditions[4]));
}

TEST_CASE("wallclock: date and time in local time")
{
    TimeZone tz("CET-1");
    CHECK(wallclock("2026-03-15T08:30:15").start == local_days{March / 15 / 2026} + 8h + 30min + 15s);
    CHECK(wallclock("2026-03-15T08:30").start == local_days{March / 15 / 2026} + 8h + 30min);
    CHECK(wallclock("2026-03-15T08").start == local_days{March / 15 / 2026} + 8h);
    CHECK(wallclock("2026-03-15").start == local_days{March / 15 / 2026});
    CHECK(wallclock("2026-03-15").repeats == 0);
}

TEST_CASE("wallclock: invalid date and time like garlic-lib")
{
    TimeZone tz("UTC");
    CHECK(wallclock("2026-02-30T08:00").start == local_days{January / 1 / 2000} + 8h);
    CHECK(wallclock("2026-03-15T25:00").start == local_days{March / 15 / 2026});
    CHECK(wallclock("2026-03-15T8:00").start == local_days{March / 15 / 2026});
}

TEST_CASE("wallclock: time zones are converted to local time")
{
    TimeZone tz("CET-1"); // UTC+1
    CHECK(wallclock("2026-03-15T08:00:00Z").start == local_days{March / 15 / 2026} + 9h);
    CHECK(wallclock("2026-03-15T08:00+03:00").start == local_days{March / 15 / 2026} + 6h);
    CHECK(wallclock("2026-03-15T23:30-05:00").start == local_days{March / 16 / 2026} + 5h + 30min);
    CHECK(wallclock("2026-03-15T08:00+01:00").start == local_days{March / 15 / 2026} + 8h);
}

TEST_CASE("wallclock: weekdays")
{
    TimeZone tz("UTC");
    // 2026-03-15 is a Sunday
    CHECK(wallclock("2026-03-15+w3T08:00").start == local_days{March / 18 / 2026} + 8h);  // next Wednesday
    CHECK(wallclock("2026-03-15-w3T08:00").start == local_days{March / 11 / 2026} + 8h);  // Wednesday before
    CHECK(wallclock("2026-03-15w1").start == local_days{March / 16 / 2026});              // without sign: after
    CHECK(wallclock("2026-03-15+w7").start == local_days{March / 22 / 2026});             // same weekday: a week later
}

TEST_CASE("wallclock: repeats and periods")
{
    TimeZone tz("UTC");

    auto endless = wallclock("R/2021-04-01T00:00:00/PT15S");
    CHECK(endless.repeats == -1);
    CHECK(endless.start == local_days{April / 1 / 2021});
    CHECK(endless.period.time == 15s);

    auto five = wallclock("R5/2026-01-01T08:00/P1Y2M3W4DT5H6M7S");
    CHECK(five.repeats == 5);
    CHECK(five.period.years == 1);
    CHECK(five.period.months == 2);
    CHECK(five.period.days == 25);
    CHECK(five.period.time == 5h + 6min + 7s);

    auto without_r = wallclock("2026-01-01T08:00/P1D"); // garlic-lib crashed here
    CHECK(without_r.start == local_days{January / 1 / 2026} + 8h);
    CHECK(without_r.period.days == 1);
    CHECK(without_r.repeats == 0);

    CHECK(wallclock("R/2026-01-01T08:00").repeats == 0); // no period: no repeat (GitHub #31)
    CHECK(wallclock("R/2026-01-01T08:00/P").repeats == 0);
    CHECK(wallclock("R3/2026-01-01/PT1H").repeats == 3);
}

TEST_CASE("wallclock is not allowed in a seq, in excl and par it is")
{
    Container body = parseBody(R"x(
        <seq><img src="a.jpg" begin="wallclock(2026-01-01T08:00)"/></seq>
        <excl><img src="b.jpg" begin="wallclock(2026-01-01T08:00)"/></excl>)x");

    const auto &in_seq = std::get<Media>(container(body.children[0]).children.at(0));
    CHECK(std::holds_alternative<Offset>(in_seq.timing.begin.at(0))); // default

    const auto &in_excl = std::get<Media>(container(container(body.children[1]).children.at(0)).children.at(0));
    CHECK(std::holds_alternative<Wallclock>(in_excl.timing.begin.at(0)));
}

TEST_CASE("end with triggers")
{
    Container body = parseBody(R"(<par><img src="a.jpg" end="accesskey(x);img2.endEvent"/></par>)");
    const auto &timing = std::get<Media>(container(body.children.at(0)).children.at(0)).timing;
    REQUIRE(timing.end.size() == 2);
    CHECK(std::holds_alternative<AccessKey>(timing.end[0]));
    CHECK(std::holds_alternative<Event>(timing.end[1]));
}

TEST_CASE("excl/trigger test files")
{
    Document document = parseFile("excl/trigger/4_accesskey.smil");
    bool     found    = false;
    std::function<void(const Container &)> search = [&](const Container &c)
    {
        for (const Node &node : c.children)
        {
            const Timing &timing = std::visit([](const auto &n) -> const Timing & { return n.timing; }, node);
            for (const auto &condition : timing.begin)
                found = found || std::holds_alternative<AccessKey>(condition);
            if (const auto *child = std::get_if<Container>(&node))
                search(*child);
        }
    };
    search(document.body);
    CHECK(found);
}
