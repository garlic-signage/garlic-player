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

using namespace garlic::core::smil;
using namespace garlic::core::tests;
using namespace std::chrono_literals;

TEST_CASE("excl without priorityClass: all children form one priorityClass with the defaults")
{
    Container body = parseBody(R"(<excl xml:id="e">
        <img xml:id="a" src="a.jpg"/>
        <seq xml:id="s"><img xml:id="b" src="b.jpg"/></seq>
    </excl>)");

    CHECK(outline(body) == "body_1_1(e(priorityClass_1_2(a s(b))))");
    const Container &excl           = container(body.children.at(0));
    const Container &priority_class = container(excl.children.at(0));
    CHECK(excl.kind == ContainerKind::Excl);
    CHECK(priority_class.kind == ContainerKind::PriorityClass);
    CHECK(priority_class.peers == Interrupt::Stop);
    CHECK(priority_class.higher == Interrupt::Pause);
    CHECK(priority_class.lower == Interrupt::Defer);
}

TEST_CASE("priorityClasses keep the order of the SMIL, the first has the highest priority")
{
    Container body = parseBody(R"(<excl xml:id="e">
        <priorityClass xml:id="high"><img xml:id="a" src="a.jpg"/></priorityClass>
        <priorityClass xml:id="low" title="Standby"><img xml:id="b" src="b.jpg"/><img xml:id="c" src="c.jpg"/></priorityClass>
    </excl>)");

    CHECK(outline(body) == "body_1_1(e(high(a) low(b c)))");
    CHECK(container(container(body.children[0]).children[1]).title == "Standby");
}

TEST_CASE("priorityClass attributes, invalid values become the default")
{
    Container body = parseBody(R"(<excl>
        <priorityClass peers="pause" higher="stop" lower="never"/>
        <priorityClass peers="defer"/>
        <priorityClass peers="never"/>
        <priorityClass peers="stop" higher="pause" lower="defer"/>
        <priorityClass peers="defer_" higher="defer" lower="stop" peer="pause" pauseDisplay="hide"/>
    </excl>)");

    const auto &classes = container(body.children.at(0)).children;
    REQUIRE(classes.size() == 5);

    CHECK(container(classes[0]).peers == Interrupt::Pause);
    CHECK(container(classes[0]).higher == Interrupt::Stop);
    CHECK(container(classes[0]).lower == Interrupt::Never);
    CHECK(container(classes[1]).peers == Interrupt::Defer);
    CHECK(container(classes[2]).peers == Interrupt::Never);
    CHECK(container(classes[3]).peers == Interrupt::Stop);
    CHECK(container(classes[3]).higher == Interrupt::Pause);
    CHECK(container(classes[3]).lower == Interrupt::Defer);

    // invalid: higher only stop/pause, lower only defer/never; "peer" is not "peers"
    CHECK(container(classes[4]).peers == Interrupt::Stop);
    CHECK(container(classes[4]).higher == Interrupt::Pause);
    CHECK(container(classes[4]).lower == Interrupt::Defer);
}

TEST_CASE("mixed children next to priorityClasses and nested priorityClasses are ignored")
{
    Container body = parseBody(R"(<excl xml:id="e">
        <img xml:id="ignored" src="x.jpg"/>
        <priorityClass xml:id="p">
            <img xml:id="a" src="a.jpg"/>
            <priorityClass xml:id="nested"><img xml:id="b" src="b.jpg"/></priorityClass>
        </priorityClass>
    </excl>
    <priorityClass xml:id="outside"><img xml:id="c" src="c.jpg"/></priorityClass>)");

    CHECK(outline(body) == "body_1_1(e(p(a)))");
}

TEST_CASE("children of an excl start by trigger: begin is indefinite by default, lists are allowed")
{
    Container body = parseBody(R"(<excl>
        <priorityClass>
            <img xml:id="a" src="a.jpg"/>
            <img xml:id="b" src="b.jpg" begin="0s;5s"/>
        </priorityClass>
    </excl>
    <excl><img xml:id="c" src="c.jpg"/></excl>)");

    const auto &a = std::get<Media>(container(container(body.children[0]).children[0]).children[0]);
    REQUIRE(a.timing.begin.size() == 1);
    CHECK(std::holds_alternative<Indefinite>(a.timing.begin[0]));

    const auto &b = std::get<Media>(container(container(body.children[0]).children[0]).children[1]);
    REQUIRE(b.timing.begin.size() == 2);
    CHECK(offset(b.timing.begin[1]) == 5s);

    const auto &c = std::get<Media>(container(container(body.children[1]).children[0]).children[0]);
    CHECK(std::holds_alternative<Indefinite>(c.timing.begin.at(0))); // also in the default priorityClass
}

TEST_CASE("excl: timing and endsync")
{
    Container body = parseBody(R"(<excl begin="0" repeatCount="indefinite" endsync="first"/>)");

    const Container &excl = container(body.children.at(0));
    CHECK(offset(excl.timing.begin.at(0)) == 0ms);
    CHECK(excl.timing.repeat_indefinite);
    CHECK(excl.endsync == EndSync::First);
    REQUIRE(excl.children.size() == 1); // empty default priorityClass
    CHECK(container(excl.children[0]).children.empty());
}

TEST_CASE("excl/higher/1_stop.smil")
{
    Document         document = parseFile("excl/higher/1_stop.smil");
    const Container &excl     = container(document.body.children.at(1));
    CHECK(excl.id == "excl_up");
    REQUIRE(excl.children.size() == 6);
    for (const Node &node : excl.children)
        CHECK(container(node).higher == Interrupt::Stop);

    const auto &img = std::get<Media>(container(excl.children[0]).children.at(0));
    CHECK(img.id == "imgA0001");
    CHECK(offset(img.timing.begin.at(0)) == 7s);
}

TEST_CASE("excl/bugs/1_nested_excl.smil: excl in a par in a priorityClass")
{
    Document         document = parseFile("excl/bugs/1_nested_excl.smil");
    const Container &outer    = container(container(document.body.children.at(0)).children.at(0));
    CHECK(outer.kind == ContainerKind::Excl);

    const Container &prio1 = container(outer.children.at(0));
    CHECK(prio1.id == "prio1");
    const Container &inner = container(container(prio1.children.at(0)).children.at(0));
    CHECK(inner.id == "error_excl");
    CHECK(container(inner.children.at(0)).id == "prio1_intern");
    CHECK(container(inner.children.at(0)).lower == Interrupt::Never);
}

TEST_CASE("all excl test files can be parsed")
{
    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(std::filesystem::path(GARLIC_CORE_TEST_DATA) / "excl"))
    {
        if (!entry.is_regular_file())
            continue;
        CAPTURE(entry.path().string());
        auto document = parse(readFile(entry.path()));
        CHECK(document.has_value());
        if (document)
            CHECK_FALSE(document->body.children.empty());
    }
}
