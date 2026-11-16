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

namespace player::ui
{
    namespace
    {
        constexpr float padding = 10.0F;

        /// Removes the last UTF-8 code point of text (all its bytes)
        void popCodePoint(std::string &text)
        {
            while (!text.empty())
            {
                auto byte = static_cast<unsigned char>(text.back());
                text.pop_back();
                if ((byte & 0xC0U) != 0x80U) // not a continuation byte: the start of the code point
                    return;
            }
        }
    }

    // ------------------------------------------------------------------ TextField

    bool TextField::input(const KeyboardInput &input)
    {
        if (!has_focus || !enabled)
            return false;
        std::string before = content;
        if (const auto *typed = std::get_if<std::string>(&input))
        {
            if (!digits_only)
                content += *typed;
            else
                for (char c : *typed)
                    if (std::isdigit(static_cast<unsigned char>(c)) != 0)
                        content += c;
        }
        else if (std::get<EditKey>(input) == EditKey::Backspace)
            popCodePoint(content);
        return content != before;
    }

    void TextField::pointerDown(Point /*position*/)
    {
        if (!enabled)
            return;
        has_focus       = true;
        focus_requested = true;
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

        constexpr float cursor_width = 2.0F;
        float           text_y       = area.y + ((area.height - canvas.textHeight()) / 2);
        float           max_width    = std::max(0.0F, inner.width - (2 * padding) - cursor_width);
        if (content.empty())
            canvas.textElided({inner.x + padding, text_y}, placeholder, look::text_dim, max_width);
        else
            canvas.textElided({inner.x + padding, text_y}, content, enabled ? look::text : look::disabled_text,
                              max_width);
        if (has_focus)
        {
            float cursor_x = inner.x + padding + std::min(canvas.textWidth(content), max_width);
            canvas.fillRect({cursor_x, text_y, cursor_width, canvas.textHeight()}, look::text);
        }
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
