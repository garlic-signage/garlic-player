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
#include "dialogs.hpp"

#include "ui/look.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>

namespace player
{
    using garlic::core::DebugInfo;
    using garlic::core::PlayerRuntimeApi;
    using garlic::core::PlayerSettings;

    namespace
    {
        constexpr float margin     = 20.0F;
        constexpr float row_height = 40.0F;
        constexpr float label_width = 150.0F;
        constexpr float button_width = 140.0F;

        std::string trimmed(const std::string &text)
        {
            auto first = text.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
                return {};
            return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
        }

        /// Like QLocale::formattedDataSize: "1.5 GiB"
        std::string bytes(std::uint64_t value)
        {
            const char *units[] = {"bytes", "KiB", "MiB", "GiB", "TiB"};
            double      size    = static_cast<double>(value);
            int         unit    = 0;
            while (size >= 1024 && unit < 4)
            {
                size /= 1024;
                ++unit;
            }
            char text[32];
            if (unit == 0)
                std::snprintf(text, sizeof(text), "%llu bytes", static_cast<unsigned long long>(value));
            else
                std::snprintf(text, sizeof(text), "%.2f %s", size, units[unit]);
            return text;
        }

        /// "400 MiB free of 1 GiB", "-" if the core does not know it
        std::string freeOf(const std::optional<std::uint64_t> &free, const std::optional<std::uint64_t> &total)
        {
            if (!free || !total)
                return "-";
            return bytes(*free) + " free of " + bytes(*total);
        }
    }

    // ------------------------------------------------------------------ Dialog

    ui::Rect Dialog::panel(const ui::Rect &window, float width, float height)
    {
        width  = std::min(width, window.width - 2 * margin);
        height = std::min(height, window.height - 2 * margin);
        return {window.x + (window.width - width) / 2, window.y + (window.height - height) / 2, width, height};
    }

    void Dialog::drawPanel(ui::Canvas &canvas, const ui::Rect &area, const std::string &title)
    {
        canvas.fillRoundedRect(area, 8.0F, ui::look::outline, ui::look::outline);
        canvas.fillRoundedRect(area.inset(1.0F), 7.0F, ui::look::dialog_top, ui::look::dialog_bottom);
        canvas.text({area.x + margin, area.y + margin}, title, ui::look::text);
    }

    // ------------------------------------------------------------------ SettingsDialog

    SettingsDialog::SettingsDialog(garlic::core::ThreadedRuntime &r, bool identity_locked) : runtime(r)
    {
        player_name.setEnabled(!identity_locked);
        index_uri.setEnabled(!identity_locked);
        index_uri.setPlaceholder("e.g. https://indexes.smil-control.com");
        httpd_port.setDigitsOnly(true);
        ok.setHighlighted(true);
        for (ui::Widget *widget : std::initializer_list<ui::Widget *>{&player_name, &index_uri, &httpd_enabled,
                                                                      &httpd_port, &ok, &cancel})
            router.add(*widget);

        auto current = ask<PlayerSettings>(runtime, [](PlayerRuntimeApi &api) { return api.playerSettings(); });
        player_name.setText(current.player_name);
        index_uri.setText(current.index_uri);
        httpd_enabled.setChecked(current.httpd_enabled);
        httpd_port.setText(std::to_string(current.httpd_port));
        httpd_port.setEnabled(current.httpd_enabled);
        auto editable = fields();
        if (!editable.empty())
            focus(editable.front());
    }

    std::vector<ui::TextField *> SettingsDialog::fields()
    {
        std::vector<ui::TextField *> list;
        for (ui::TextField *field : {&player_name, &index_uri, &httpd_port})
            if (field->isEnabled())
                list.push_back(field);
        return list;
    }

    void SettingsDialog::focus(ui::TextField *field)
    {
        for (ui::TextField *each : {&player_name, &index_uri, &httpd_port})
            each->setFocused(each == field);
    }

    void SettingsDialog::layout(const ui::Rect &window)
    {
        float error_rows = error.empty() ? 0 : 1;
        area             = panel(window, 640, margin * 3 + row_height * (6 + error_rows));
        float x          = area.x + margin + label_width;
        float width      = area.width - 2 * margin - label_width;
        float y          = area.y + margin + row_height * (1 + error_rows);
        for (ui::Widget *widget : std::initializer_list<ui::Widget *>{&player_name, &index_uri, &httpd_enabled, &httpd_port})
        {
            widget->setBounds({x, y + 2, width, row_height - 6});
            y += row_height;
        }
        float bottom = area.y + area.height - margin - row_height;
        cancel.setBounds({area.x + area.width - margin - button_width, bottom, button_width, row_height});
        ok.setBounds({area.x + area.width - 2 * margin - 2 * button_width, bottom, button_width, row_height});
    }

    void SettingsDialog::draw(ui::Canvas &canvas) const
    {
        drawPanel(canvas, area, "garlic-player settings");
        float y = area.y + margin + row_height;
        if (!error.empty())
        {
            canvas.textElided({area.x + margin, y}, error, ui::look::error, area.width - 2 * margin);
            y += row_height;
        }
        for (const char *label : {"Player name", "Content URL", "HTTP server", "Port"})
        {
            canvas.text({area.x + margin, y + (row_height - canvas.textHeight()) / 2}, label, ui::look::text);
            y += row_height;
        }
        for (const ui::Widget *widget : std::initializer_list<const ui::Widget *>{&player_name, &index_uri,
                                                                                  &httpd_enabled, &httpd_port, &ok,
                                                                                  &cancel})
            widget->draw(canvas);
    }

    void SettingsDialog::pointer(const ui::PointerEvent &event)
    {
        router.handle(event);
        for (ui::TextField *field : {&player_name, &index_uri, &httpd_port})
            if (field->takeFocusRequest())
                focus(field);
        if (httpd_enabled.takeToggled())
        {
            httpd_port.setEnabled(httpd_enabled.checked());
            if (!httpd_enabled.checked())
                httpd_port.setFocused(false);
        }
        if (ok.takePress())
            accept();
        if (cancel.takePress())
            state = Result::Rejected;
    }

    std::string SettingsDialog::selectedText() const
    {
        for (const ui::TextField *field : {&player_name, &index_uri, &httpd_port})
            if (field->focused())
                return field->selectedText();
        return {};
    }

    void SettingsDialog::input(const ui::KeyboardInput &input)
    {
        if (const auto *key = std::get_if<ui::EditKey>(&input))
        {
            if (*key == ui::EditKey::Enter)
                return accept();
            if (*key == ui::EditKey::Escape)
            {
                state = Result::Rejected;
                return;
            }
            if (*key == ui::EditKey::Tab || *key == ui::EditKey::BackTab)
            {
                auto list = fields();
                if (list.empty())
                    return;
                auto current = std::ranges::find_if(list, [](ui::TextField *field) { return field->focused(); });
                std::size_t index = current == list.end() ? 0 : static_cast<std::size_t>(current - list.begin());
                index = *key == ui::EditKey::Tab ? (index + 1) % list.size() : (index + list.size() - 1) % list.size();
                focus(list[index]);
                return;
            }
        }
        for (ui::TextField *field : {&player_name, &index_uri, &httpd_port})
            field->input(input);
    }

    /// Closes only when the core has stored the settings; otherwise its error text is shown (garlic-lib).
    void SettingsDialog::accept()
    {
        unsigned port_number = 0;
        std::string port     = httpd_port.text();
        std::from_chars(port.data(), port.data() + port.size(), port_number);
        PlayerSettings next{trimmed(player_name.text()), trimmed(index_uri.text()), httpd_enabled.checked(),
                            static_cast<std::uint16_t>(std::clamp(port_number, 1U, 65535U))};
        auto changed = ask<std::expected<void, std::string>>(
            runtime, [next](PlayerRuntimeApi &api) { return api.changePlayerSettings(next); });
        if (!changed)
        {
            error = changed.error();
            return;
        }
        state = Result::Accepted;
    }

    // ------------------------------------------------------------------ DebugDialog

    DebugDialog::DebugDialog(garlic::core::ThreadedRuntime &r, Playing p) : runtime(r), playing(std::move(p))
    {
        close.setHighlighted(true);
        router.add(quit);
        router.add(close);
        refresh();
    }

    void DebugDialog::refresh()
    {
        DebugInfo info = ask<DebugInfo>(runtime, [](PlayerRuntimeApi &api) { return api.debugInfo(); });
        rows           = {
            {"Playlist", info.index_title.empty() ? "-" : info.index_title},
            {"User agent", info.user_agent},
            {"Disk (cache)", freeOf(info.disk_free, info.disk_total)},
            {"System memory", freeOf(info.memory_free, info.memory_total)},
            {"Player memory", info.memory_used == 0 ? "-"
                                                    : bytes(info.memory_used) + ", max " + bytes(info.memory_max) +
                                                          " at " + info.memory_max_time},
            {"Threads", info.threads == 0 ? "-"
                                          : std::to_string(info.threads) + ", max " + std::to_string(info.threads_max)},
        };
        // the running downloads: "title  42 % (136.0 MB of 323.0 MB)", then how many wait
        std::vector<std::string> loading;
        for (const auto &download : info.downloads)
            loading.push_back(download.total == 0
                                  ? download.name + "  " + bytes(download.received)
                                  : download.name + "  " + std::to_string(download.received * 100 / download.total) +
                                        " % (" + bytes(download.received) + " of " + bytes(download.total) + ")");
        if (info.downloads_waiting > 0)
            loading.push_back("+" + std::to_string(info.downloads_waiting) + " waiting");
        if (loading.empty())
            loading.push_back("-");
        for (std::size_t i = 0; i < loading.size(); ++i)
            rows.emplace_back(i == 0 ? "Downloading" : "", loading[i]);
        media = playing ? playing() : std::vector<std::string>();
    }

    void DebugDialog::layout(const ui::Rect &window)
    {
        float lines = static_cast<float>(rows.size() + std::max<std::size_t>(media.size(), 1));
        area        = panel(window, 760, margin * 3 + row_height * (2 + lines * 0.75F));
        float bottom = area.y + area.height - margin - row_height;
        quit.setBounds({area.x + margin, bottom, button_width, row_height});
        close.setBounds({area.x + area.width - margin - button_width, bottom, button_width, row_height});
    }

    void DebugDialog::draw(ui::Canvas &canvas) const
    {
        drawPanel(canvas, area, "Information");
        float line  = row_height * 0.75F;
        float y     = area.y + margin + row_height;
        float x     = area.x + margin + label_width;
        float width = area.width - 2 * margin - label_width;
        for (const auto &[label, value] : rows)
        {
            canvas.text({area.x + margin, y}, label, ui::look::text_dim);
            canvas.textElided({x, y}, value, ui::look::text, width);
            y += line;
        }
        canvas.text({area.x + margin, y}, "Playing", ui::look::text_dim);
        if (media.empty())
            canvas.text({x, y}, "-", ui::look::text);
        for (const std::string &each : media)
        {
            canvas.textElided({x, y}, each, ui::look::text, width);
            y += line;
        }
        quit.draw(canvas);
        close.draw(canvas);
    }

    void DebugDialog::input(const ui::KeyboardInput &input)
    {
        if (const auto *key = std::get_if<ui::EditKey>(&input);
            key != nullptr && (*key == ui::EditKey::Escape || *key == ui::EditKey::Enter))
            state = Result::Rejected;
    }

    void DebugDialog::check()
    {
        if (quit.takePress())
            state = Result::QuitPlayer;
        if (close.takePress())
            state = Result::Rejected;
    }
}
