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
#ifndef PLAYER_UI_SDL_UI_LOOK_HPP
#define PLAYER_UI_SDL_UI_LOOK_HPP

#include "canvas.hpp"

/// The colors and shapes of the dark look of the DJ workstation, only what the menu and the dialogs use.
namespace player::ui::look
{
    inline constexpr Color dialog_top{40, 42, 46};
    inline constexpr Color dialog_bottom{24, 25, 28};
    inline constexpr Color frame{70, 72, 78};
    inline constexpr Color outline{14, 14, 16};
    inline constexpr Color text{232, 232, 235};
    inline constexpr Color text_dim{150, 152, 158};
    inline constexpr Color error{235, 90, 90};
    inline constexpr Color button_gray{68, 70, 75};
    inline constexpr Color button_blue{40, 75, 120};
    inline constexpr Color disabled_background{42, 42, 42};
    inline constexpr Color disabled_text{110, 110, 110};
    inline constexpr Color field{35, 35, 35};
    inline constexpr Color field_top{16, 17, 20};
    inline constexpr Color field_bottom{30, 32, 36};
    inline constexpr Color field_frame{70, 70, 70};
    inline constexpr Color highlight_top{62, 112, 172};
    inline constexpr Color highlight_bottom{36, 72, 118};
    inline constexpr Color highlight_text{255, 255, 255};
    inline constexpr Color selection{40, 90, 150}; // selected text in a text field
    inline constexpr Color item_top{64, 66, 72};
    inline constexpr Color item_bottom{50, 52, 57};
    inline constexpr Color focus{80, 160, 255};
    inline constexpr Color check{0, 190, 90};

    /// A color moved towards white (amount > 0) or black (amount < 0)
    [[nodiscard]] Color shade(Color color, int amount) noexcept;

    /// A button body: rounded, glossy gradient around base, a dark outline; pressed it is darker and reversed
    void raised(Canvas &canvas, const Rect &area, Color base, bool pressed, float radius = 8.0F);

    /// A recessed panel with a thin frame, for check boxes
    void sunken(Canvas &canvas, const Rect &area, Color fill, float radius = 6.0F);
}

#endif // PLAYER_UI_SDL_UI_LOOK_HPP
