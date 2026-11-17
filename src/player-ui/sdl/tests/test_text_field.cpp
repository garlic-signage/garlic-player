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
#include "ui/controls.hpp"

#include <doctest/doctest.h>

using player::ui::EditKey;
using player::ui::TextField;

namespace
{
    /// A focused field with text, the cursor at the end
    struct Field
    {
        TextField field;

        explicit Field(std::string text)
        {
            field.setText(std::move(text));
            field.setFocused(true);
        }

        Field &key(EditKey key)
        {
            field.input(key);
            return *this;
        }

        Field &type(const std::string &text)
        {
            field.input(text);
            return *this;
        }

        [[nodiscard]] const std::string &text() const { return field.text(); }
    };
}

TEST_CASE("typing goes in at the cursor, which the arrows, Home and End move")
{
    Field f("http://hub/index.smil");
    f.key(EditKey::Home).type("x");
    CHECK(f.text() == "xhttp://hub/index.smil");
    f.key(EditKey::End).key(EditKey::Left).key(EditKey::Left).type("Y");
    CHECK(f.text() == "xhttp://hub/index.smYil");
    f.key(EditKey::Backspace).key(EditKey::Delete);
    CHECK(f.text() == "xhttp://hub/index.sml");
    f.key(EditKey::Right).key(EditKey::Right).key(EditKey::Right).type("!"); // stops at the end
    CHECK(f.text() == "xhttp://hub/index.sml!");
}

TEST_CASE("selection: Shift selects, typing and Backspace replace it, an arrow ends it at its side")
{
    Field f("garlic-player");
    f.key(EditKey::SelectHome);
    CHECK(f.field.selectedText() == "garlic-player");
    f.type("new");
    CHECK(f.text() == "new");

    Field g("abcdef");
    g.key(EditKey::SelectLeft).key(EditKey::SelectLeft).key(EditKey::Backspace);
    CHECK(g.text() == "abcd");
    g.key(EditKey::Home).key(EditKey::SelectRight).key(EditKey::SelectRight).key(EditKey::Right).type("|");
    CHECK(g.text() == "ab|cd"); // Right without Shift goes to the right end of the selection
}

TEST_CASE("Ctrl+A selects all, Delete clears it; Ctrl+arrows jump by words")
{
    Field f("http://hub/index.smil");
    f.key(EditKey::SelectAll);
    CHECK(f.field.selectedText() == "http://hub/index.smil");
    f.key(EditKey::Delete);
    CHECK(f.text().empty());

    Field g("http://hub/index.smil");
    g.key(EditKey::WordLeft).type("X"); // before "smil"
    CHECK(g.text() == "http://hub/index.Xsmil");
    g.key(EditKey::Home).key(EditKey::WordRight).type("|"); // after "http://"
    CHECK(g.text() == "http://|hub/index.Xsmil");
    g.key(EditKey::SelectWordRight);
    CHECK(g.field.selectedText() == "hub/");
}

TEST_CASE("UTF-8: the cursor and Backspace move by whole characters; digits only for a port; pasting one line")
{
    Field f("Grüße");
    f.key(EditKey::Left).key(EditKey::Left).key(EditKey::Backspace); // cursor in "Grü|ße": removes the ü, both bytes
    CHECK(f.text() == "Grße");

    Field port("8080");
    port.field.setDigitsOnly(true);
    port.key(EditKey::SelectAll).type("90a9x1");
    CHECK(port.text() == "9091");

    Field pasted("");
    pasted.type("line one\nline two");
    CHECK(pasted.text() == "line oneline two");
}

TEST_CASE("unfocused or disabled: no input")
{
    TextField field;
    field.setText("abc");
    CHECK_FALSE(field.input(std::string("x")));
    field.setFocused(true);
    field.setEnabled(false);
    CHECK_FALSE(field.input(EditKey::Backspace));
    CHECK(field.text() == "abc");
}
