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
#ifndef PLAYER_UI_SDL_UI_CONTROLS_HPP
#define PLAYER_UI_SDL_UI_CONTROLS_HPP

#include "widget.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

/// The controls of the menu and the dialogs, copied from the DJ workstation (text_field, check_box, text_button,
/// context_menu) and cut down to what the player needs.
namespace player::ui
{
    /// Keys that edit text or move between fields, apart from typing. Select… moves the cursor and extends the
    /// selection (Shift), Word… jumps by words (Ctrl).
    enum class EditKey : std::uint8_t
    {
        Backspace,
        Delete,
        Enter,
        Escape,
        Tab,
        BackTab,
        Left,
        Right,
        WordLeft,
        WordRight,
        Home,
        End,
        SelectLeft,
        SelectRight,
        SelectWordLeft,
        SelectWordRight,
        SelectHome,
        SelectEnd,
        SelectAll
    };

    /// Typed text (UTF-8, after input methods) or an editing key
    using KeyboardInput = std::variant<std::string, EditKey>;

    /**
     * One-line text input like a desktop line edit: a cursor that moves with the arrow keys, Home and End (by words
     * with Ctrl), a selection with Shift or by dragging, a double click selects all; typed or pasted text replaces the
     * selection. The text scrolls when it is longer than the field. The clipboard is the business of the window
     * (selectedText(), pasting is typing).
     */
    class TextField final : public Widget
    {
        public:
            [[nodiscard]] const std::string &text() const noexcept { return content; }
            void                             setText(std::string text)
            {
                content = std::move(text);
                cursor  = content.size();
                anchor  = cursor;
            }
            /// The selected part, empty without a selection
            [[nodiscard]] std::string selectedText() const;
            void                             setPlaceholder(std::string hint) { placeholder = std::move(hint); }
            void                             setEnabled(bool on) noexcept { enabled = on; }
            [[nodiscard]] bool               isEnabled() const noexcept { return enabled; }
            /// Only digits are typed, e.g. a port
            void                             setDigitsOnly(bool on) noexcept { digits_only = on; }

            [[nodiscard]] bool focused() const noexcept { return has_focus; }
            void               setFocused(bool focused) noexcept { has_focus = focused && enabled; }
            /// True once after a tap focused the field, so the dialog can take the focus from the other fields
            [[nodiscard]] bool takeFocusRequest() noexcept { return std::exchange(focus_requested, false); }

            /// Applies the input if focused; true when the text changed
            bool input(const KeyboardInput &input);

            void pointerDown(Point position) override;
            void pointerMove(Point position) override;
            void pointerUp(Point /*position*/) override { dragging = false; }
            void draw(Canvas &canvas) const override;

        private:
            std::string placeholder;
            std::string content;
            std::size_t cursor          = 0; // byte offset at the start of a code point
            std::size_t anchor          = 0; // the other end of the selection; == cursor: no selection
            bool        enabled         = true;
            bool        digits_only     = false;
            bool        has_focus       = false;
            bool        focus_requested = false;
            bool        dragging        = false;
            std::int64_t last_down_ms   = -1000; // for the double click

            // what draw() laid out, for the pointer: the x of every code point start and of the end, the scroll
            mutable std::vector<std::pair<std::size_t, float>> stops; // byte offset → x in the window
            mutable float                                      scroll = 0.0F;

            void        insert(std::string_view text);
            bool        eraseSelection();
            void        move(std::size_t to, bool select);
            std::size_t offsetAt(float x) const;
    };

    /// A box with a label that is on or off; a tap on the box or its label toggles it
    class CheckBox final : public Widget
    {
        public:
            explicit CheckBox(std::string label = {}) : text{std::move(label)} {}

            [[nodiscard]] bool checked() const noexcept { return on; }
            void               setChecked(bool checked) noexcept { on = checked; }
            /// True once after the user toggled it
            [[nodiscard]] bool takeToggled() noexcept { return std::exchange(toggled, false); }

            void pointerDown(Point /*position*/) override { down = true; }
            void pointerMove(Point /*position*/) override {}
            void pointerUp(Point position) override;
            void draw(Canvas &canvas) const override;

        private:
            std::string text;
            bool        on      = false;
            bool        down    = false;
            bool        toggled = false;
    };

    /// A button with a text label. A press counts when the pointer goes down and up on the button.
    class TextButton final : public Widget
    {
        public:
            explicit TextButton(std::string label = {}) : text{std::move(label)} {}

            /// Drawn in the accent color: the default button of a dialog
            void setHighlighted(bool on) noexcept { highlighted = on; }

            /// True once after a press
            [[nodiscard]] bool takePress() noexcept { return std::exchange(pressed, false); }

            void pointerDown(Point /*position*/) override { down = true; }
            void pointerMove(Point /*position*/) override {}
            void pointerUp(Point position) override;
            void draw(Canvas &canvas) const override;

        private:
            std::string text;
            bool        highlighted = false;
            bool        down        = false;
            bool        pressed     = false;
    };

    struct MenuItem
    {
            int         id = 0;
            std::string label;
            std::string shortcut; ///< shown right-aligned, e.g. "Ctrl+S"
    };

    /**
     * A menu that opens at a point. It covers the whole window while it is open, so a touch anywhere else only closes
     * it; a choice counts when the pointer goes down and up on the same entry. Moved so that it lies inside the window.
     */
    class ContextMenu final : public Widget
    {
        public:
            static constexpr float item_height = 44.0F;
            static constexpr float menu_width  = 280.0F;

            void setItems(std::vector<MenuItem> new_items) { items = std::move(new_items); }

            /// Opens at position inside window_area
            void               open(Point position, const Rect &window_area);
            void               close();
            [[nodiscard]] bool isOpen() const noexcept { return opened; }

            /// The id of the entry chosen, once; also when it closed without a choice: std::nullopt
            [[nodiscard]] std::optional<int> takeChosen() noexcept { return std::exchange(chosen, std::nullopt); }

            void pointerDown(Point position) override;
            void pointerMove(Point /*position*/) override {}
            void pointerUp(Point position) override;
            void draw(Canvas &canvas) const override;

        private:
            static constexpr std::size_t none    = static_cast<std::size_t>(-1);
            static constexpr std::size_t outside = none - 1;

            [[nodiscard]] Rect        menuArea() const;
            [[nodiscard]] Rect        itemArea(std::size_t index) const;
            [[nodiscard]] std::size_t hit(Point position) const;

            std::vector<MenuItem> items;
            Rect                  window;
            Point                 at;
            bool                  opened  = false;
            std::size_t           pressed = none;
            std::optional<int>    chosen;
    };
}

#endif // PLAYER_UI_SDL_UI_CONTROLS_HPP
