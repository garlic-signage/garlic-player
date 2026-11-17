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
#include "controls.hpp"
#include "look.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <chrono>

namespace player::ui
{
    namespace
    {
        constexpr float padding         = 10.0F;
        constexpr float cursor_width    = 2.0F;
        constexpr int   double_click_ms = 400;

        bool isContinuation(char byte)
        {
            return (static_cast<unsigned char>(byte) & 0xC0U) == 0x80U;
        }

        /// The start of the code point before / after offset (UTF-8)
        std::size_t previousPoint(const std::string &text, std::size_t offset)
        {
            if (offset == 0)
                return 0;
            --offset;
            while (offset > 0 && isContinuation(text[offset]))
                --offset;
            return offset;
        }

        std::size_t nextPoint(const std::string &text, std::size_t offset)
        {
            if (offset >= text.size())
                return text.size();
            ++offset;
            while (offset < text.size() && isContinuation(text[offset]))
                ++offset;
            return offset;
        }

        /// Words for Ctrl+arrows: letters and digits (and every non-ASCII byte); "/", ".", ":", "-", " " … separate
        bool isWordByte(char byte)
        {
            auto c = static_cast<unsigned char>(byte);
            return c >= 0x80U || std::isalnum(c) != 0 || c == '_';
        }

        std::size_t previousWord(const std::string &text, std::size_t offset)
        {
            while (offset > 0 && !isWordByte(text[offset - 1]))
                --offset;
            while (offset > 0 && isWordByte(text[offset - 1])) // all bytes of a non-ASCII code point are word bytes
                --offset;
            return offset;
        }

        std::size_t nextWord(const std::string &text, std::size_t offset)
        {
            while (offset < text.size() && isWordByte(text[offset]))
                ++offset;
            while (offset < text.size() && !isWordByte(text[offset]))
                ++offset;
            return offset;
        }

        std::int64_t nowMs()
        {
            using namespace std::chrono;
            return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        }
    }

    // ------------------------------------------------------------------ TextField

    std::string TextField::selectedText() const
    {
        auto [from, to] = std::minmax(cursor, anchor);
        return content.substr(from, to - from);
    }

    bool TextField::eraseSelection()
    {
        if (cursor == anchor)
            return false;
        auto [from, to] = std::minmax(cursor, anchor);
        content.erase(from, to - from);
        cursor = anchor = from;
        return true;
    }

    void TextField::insert(std::string_view text)
    {
        std::string accepted;
        for (char c : text)
            if (!digits_only || std::isdigit(static_cast<unsigned char>(c)) != 0)
                if (c != '\n' && c != '\r') // one line: pasted line breaks are dropped
                    accepted += c;
        if (accepted.empty())
            return;
        eraseSelection();
        content.insert(cursor, accepted);
        cursor += accepted.size();
        anchor = cursor;
    }

    void TextField::move(std::size_t to, bool select)
    {
        cursor = std::min(to, content.size());
        if (!select)
            anchor = cursor;
    }

    bool TextField::input(const KeyboardInput &input)
    {
        if (!has_focus || !enabled)
            return false;
        std::string before = content;
        if (const auto *typed = std::get_if<std::string>(&input))
        {
            insert(*typed);
            return content != before;
        }
        bool selection = cursor != anchor;
        auto [from, to] = std::minmax(cursor, anchor);
        switch (std::get<EditKey>(input))
        {
            case EditKey::Backspace:
                if (!eraseSelection() && cursor > 0)
                {
                    std::size_t start = previousPoint(content, cursor);
                    content.erase(start, cursor - start);
                    cursor = anchor = start;
                }
                break;
            case EditKey::Delete:
                if (!eraseSelection() && cursor < content.size())
                    content.erase(cursor, nextPoint(content, cursor) - cursor);
                break;
            // without Shift an arrow ends a selection at its side, like the line edits of the desktops
            case EditKey::Left: move(selection ? from : previousPoint(content, cursor), false); break;
            case EditKey::Right: move(selection ? to : nextPoint(content, cursor), false); break;
            case EditKey::WordLeft: move(previousWord(content, cursor), false); break;
            case EditKey::WordRight: move(nextWord(content, cursor), false); break;
            case EditKey::Home: move(0, false); break;
            case EditKey::End: move(content.size(), false); break;
            case EditKey::SelectLeft: move(previousPoint(content, cursor), true); break;
            case EditKey::SelectRight: move(nextPoint(content, cursor), true); break;
            case EditKey::SelectWordLeft: move(previousWord(content, cursor), true); break;
            case EditKey::SelectWordRight: move(nextWord(content, cursor), true); break;
            case EditKey::SelectHome: move(0, true); break;
            case EditKey::SelectEnd: move(content.size(), true); break;
            case EditKey::SelectAll:
                anchor = 0;
                cursor = content.size();
                break;
            default: break; // Enter, Escape, Tab: the business of the dialog
        }
        return content != before;
    }

    std::size_t TextField::offsetAt(float x) const
    {
        if (stops.empty())
            return content.size();
        // the nearest code point boundary
        std::size_t best      = stops.front().first;
        float       best_dist = std::abs(stops.front().second - x);
        for (const auto &[offset, stop_x] : stops)
            if (float dist = std::abs(stop_x - x); dist < best_dist)
            {
                best      = offset;
                best_dist = dist;
            }
        return best;
    }

    void TextField::pointerDown(Point position)
    {
        if (!enabled)
            return;
        bool was_focused = has_focus;
        has_focus        = true;
        focus_requested  = true;
        std::int64_t now = nowMs();
        if (was_focused && now - last_down_ms <= double_click_ms)
        {
            // a double click selects all (URLs, names: mostly replaced as a whole)
            anchor       = 0;
            cursor       = content.size();
            last_down_ms = -1000;
            dragging     = false;
            return;
        }
        last_down_ms = now;
        move(offsetAt(position.x), false);
        dragging = true;
    }

    void TextField::pointerMove(Point position)
    {
        if (dragging)
            move(offsetAt(position.x), true);
    }

    void TextField::draw(Canvas &canvas) const
    {
        const Rect &area = bounds();
        Color       edge = has_focus ? look::focus : look::field_frame;
        canvas.fillRoundedRect(area, 5.0F, edge, edge);
        Rect inner = area.inset(2.0F);
        if (enabled)
            canvas.fillRoundedRect(inner, 4.0F, look::field_top, look::field_bottom);
        else
            canvas.fillRoundedRect(inner, 4.0F, look::disabled_background, look::disabled_background);

        float text_y    = area.y + ((area.height - canvas.textHeight()) / 2);
        float max_width = std::max(0.0F, inner.width - (2 * padding) - cursor_width);
        float left      = inner.x + padding;
        if (content.empty())
        {
            stops = {{0, left}};
            scroll = 0.0F;
            canvas.textElided({left, text_y}, placeholder, look::text_dim, max_width);
            if (has_focus)
                canvas.fillRect({left, text_y, cursor_width, canvas.textHeight()}, look::text);
            return;
        }
        auto layout = [&]
        {
            stops.clear();
            for (std::size_t offset = 0;; offset = nextPoint(content, offset))
            {
                stops.emplace_back(offset,
                                   left - scroll + canvas.textWidth(std::string_view(content).substr(0, offset)));
                if (offset >= content.size())
                    break;
            }
        };
        if (!has_focus) // from the start, elided like before; the stops let the first click place the cursor
        {
            scroll = 0.0F;
            layout();
            canvas.textElided({left, text_y}, content, enabled ? look::text : look::disabled_text, max_width);
            return;
        }

        // scrolled so the cursor stays visible
        float cursor_x = canvas.textWidth(std::string_view(content).substr(0, cursor));
        if (cursor_x - scroll > max_width)
            scroll = cursor_x - max_width;
        if (cursor_x < scroll)
            scroll = cursor_x;
        scroll = std::clamp(scroll, 0.0F, std::max(0.0F, canvas.textWidth(content) - max_width));

        layout();

        canvas.setClip(Rect{left, inner.y, max_width + cursor_width, inner.height});
        if (cursor != anchor)
        {
            auto [from, to] = std::minmax(cursor, anchor);
            float x1        = left - scroll + canvas.textWidth(std::string_view(content).substr(0, from));
            float x2        = left - scroll + canvas.textWidth(std::string_view(content).substr(0, to));
            canvas.fillRect({x1, text_y, x2 - x1, canvas.textHeight()}, look::selection);
        }
        canvas.text({left - scroll, text_y}, content, enabled ? look::text : look::disabled_text);
        canvas.fillRect({left - scroll + cursor_x, text_y, cursor_width, canvas.textHeight()}, look::text);
        canvas.setClip(std::nullopt);
    }

    // ------------------------------------------------------------------ CheckBox

    void CheckBox::pointerUp(Point position)
    {
        if (down && bounds().contains(position))
        {
            on      = !on;
            toggled = true;
        }
        down = false;
    }

    void CheckBox::draw(Canvas &canvas) const
    {
        const Rect &area = bounds();
        if (area.width <= 0.0F || area.height <= 0.0F)
            return;
        constexpr float box_size = 22.0F;
        constexpr float gap      = 8.0F;
        float           size     = std::min(box_size, area.height);
        Rect            box{area.x, area.y + ((area.height - size) / 2), size, size};
        look::sunken(canvas, box, look::field, 5.0F);
        if (on)
            canvas.fillRoundedRect(box.inset(5.0F), 3.0F, look::check, look::shade(look::check, -40));
        float text_x = area.x + size + gap;
        canvas.textElided({text_x, area.y + ((area.height - canvas.textHeight()) / 2)}, text, look::text,
                          std::max(0.0F, area.x + area.width - text_x));
    }

    // ------------------------------------------------------------------ TextButton

    void TextButton::pointerUp(Point position)
    {
        pressed = down && bounds().contains(position);
        down    = false;
    }

    void TextButton::draw(Canvas &canvas) const
    {
        const Rect &area = bounds();
        if (area.width <= 0.0F || area.height <= 0.0F)
            return;
        look::raised(canvas, area, highlighted ? look::button_blue : look::button_gray, down);
        float room  = std::max(0.0F, area.width - (2 * padding));
        float width = std::min(canvas.textWidth(text), room);
        canvas.textElided({area.x + ((area.width - width) / 2), area.y + ((area.height - canvas.textHeight()) / 2)},
                          text, look::text, room);
    }

    // ------------------------------------------------------------------ ContextMenu

    void ContextMenu::open(Point position, const Rect &window_area)
    {
        if (items.empty())
            return;
        window  = window_area;
        at      = position;
        opened  = true;
        pressed = none;
        chosen.reset();
        setBounds(window);
    }

    void ContextMenu::close()
    {
        opened  = false;
        pressed = none;
        setBounds({});
    }

    Rect ContextMenu::menuArea() const
    {
        float height = static_cast<float>(items.size()) * item_height;
        float width  = std::min(menu_width, window.width);
        float x      = std::clamp(at.x, window.x, std::max(window.x, window.x + window.width - width));
        float y      = std::clamp(at.y, window.y, std::max(window.y, window.y + window.height - height));
        return {x, y, width, height};
    }

    Rect ContextMenu::itemArea(std::size_t index) const
    {
        Rect menu = menuArea();
        return {menu.x, menu.y + (static_cast<float>(index) * item_height), menu.width, item_height};
    }

    std::size_t ContextMenu::hit(Point position) const
    {
        if (!opened)
            return none;
        for (std::size_t index = 0; index < items.size(); ++index)
            if (itemArea(index).contains(position))
                return index;
        return outside;
    }

    void ContextMenu::pointerDown(Point position)
    {
        pressed = hit(position);
    }

    void ContextMenu::pointerUp(Point position)
    {
        std::size_t target = hit(position);
        std::size_t down   = std::exchange(pressed, none);
        if (target != down || target == none)
            return;
        if (target != outside)
            chosen = items[target].id;
        close();
    }

    void ContextMenu::draw(Canvas &canvas) const
    {
        if (!opened)
            return;
        canvas.fillRoundedRect(menuArea(), 4.0F, look::outline, look::outline);
        for (std::size_t index = 0; index < items.size(); ++index)
        {
            Rect area = itemArea(index);
            bool is_pressed = pressed == index;
            canvas.fillRoundedRect(area.inset(1.0F), 0.0F, is_pressed ? look::highlight_top : look::item_top,
                                   is_pressed ? look::highlight_bottom : look::item_bottom);
            float y        = area.y + ((area.height - canvas.textHeight()) / 2);
            float shortcut = canvas.textWidth(items[index].shortcut);
            canvas.text({area.x + area.width - padding - shortcut, y}, items[index].shortcut, look::text_dim);
            canvas.textElided({area.x + padding, y}, items[index].label,
                              is_pressed ? look::highlight_text : look::text,
                              std::max(0.0F, area.width - (3 * padding) - shortcut));
        }
    }
}
