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
#ifndef PLAYER_UI_SDL_DIALOGS_HPP
#define PLAYER_UI_SDL_DIALOGS_HPP

#include "ui/controls.hpp"
#include "ui/pointer_router.hpp"

#include "player_runtime/threaded_runtime.hpp"

#include <chrono>
#include <functional>
#include <future>
#include <string>
#include <vector>

namespace player
{
    /// Runs call in the core thread and waits for its result.
    template <typename Result>
    Result ask(garlic::core::ThreadedRuntime &runtime, std::function<Result(garlic::core::PlayerRuntimeApi &)> call)
    {
        std::promise<Result> answer;
        runtime.post([&](garlic::core::PlayerRuntimeApi &r) { answer.set_value(call(r)); });
        return answer.get_future().get();
    }

    /**
     * A dialog drawn over the playing content, modal like the dialogs of the qwidget player: while it is open, the
     * pointer and the keyboard go to it. The window lays it out in points, centered.
     */
    class Dialog
    {
        public:
            enum class Result
            {
                Open,
                Accepted,
                Rejected,
                QuitPlayer // the debug dialog: "Quit player"
            };

            virtual ~Dialog() = default;

            /// Lays the dialog out in window (points)
            virtual void layout(const ui::Rect &window) = 0;
            virtual void draw(ui::Canvas &canvas) const = 0;
            virtual void pointer(const ui::PointerEvent &event) = 0;
            virtual void input(const ui::KeyboardInput &input) = 0;
            /// Once a second, e.g. to refresh what it shows
            virtual void tick() {}
            /// Whether it takes typed text (the window starts text input then)
            [[nodiscard]] virtual bool wantsText() const { return false; }
            /// The selected text of the focused field, for the clipboard (Ctrl+C, Ctrl+X)
            [[nodiscard]] virtual std::string selectedText() const { return {}; }

            [[nodiscard]] Result result() const noexcept { return state; }

        protected:
            Result state = Result::Open;

            /// The panel with its title, centered in window; returns the area inside for the content
            static ui::Rect panel(const ui::Rect &window, float width, float height);
            static void     drawPanel(ui::Canvas &canvas, const ui::Rect &area, const std::string &title);
    };

    /// Player name, content URL and the HTTP server, like the SettingsDialog of the qwidget player (garlic-lib).
    class SettingsDialog final : public Dialog
    {
        public:
            /// identity_locked: player name and content URL come from the supervisor, they are only shown
            SettingsDialog(garlic::core::ThreadedRuntime &runtime, bool identity_locked);

            void layout(const ui::Rect &window) override;
            void draw(ui::Canvas &canvas) const override;
            void pointer(const ui::PointerEvent &event) override;
            void input(const ui::KeyboardInput &input) override;
            [[nodiscard]] bool wantsText() const override { return true; }
            [[nodiscard]] std::string selectedText() const override;

        private:
            garlic::core::ThreadedRuntime &runtime;
            ui::Rect                       area;
            ui::TextField                  player_name;
            ui::TextField                  index_uri;
            ui::CheckBox                   httpd_enabled{"active"};
            ui::TextField                  httpd_port;
            ui::TextButton                 ok{"OK"};
            ui::TextButton                 cancel{"Cancel"};
            ui::PointerRouter              router;
            std::string                    error;

            std::vector<ui::TextField *> fields();
            void                         focus(ui::TextField *field);
            void                         accept();
    };

    /// What the player does now, like the DebugDialog of the qwidget player: refreshed every second.
    class DebugDialog final : public Dialog
    {
        public:
            using Playing = std::function<std::vector<std::string>()>;

            DebugDialog(garlic::core::ThreadedRuntime &runtime, Playing playing);

            void layout(const ui::Rect &window) override;
            void draw(ui::Canvas &canvas) const override;
            void pointer(const ui::PointerEvent &event) override { router.handle(event); check(); }
            void input(const ui::KeyboardInput &input) override;
            void tick() override { refresh(); }

        private:
            garlic::core::ThreadedRuntime &runtime;
            Playing                        playing;
            ui::Rect                       area;
            ui::TextButton                 quit{"Quit player"};
            ui::TextButton                 close{"Close"};
            ui::PointerRouter              router;
            std::vector<std::pair<std::string, std::string>> rows;
            std::vector<std::string>                         media;

            void refresh();
            void check();
    };
}

#endif // PLAYER_UI_SDL_DIALOGS_HPP
