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
#include "look.hpp"

#include <algorithm>

namespace player::ui::look
{
    Color shade(Color color, int amount) noexcept
    {
        auto move = [amount](unsigned char value)
        { return static_cast<unsigned char>(std::clamp(static_cast<int>(value) + amount, 0, 255)); };
        return {move(color.red), move(color.green), move(color.blue), color.alpha};
    }

    void raised(Canvas &canvas, const Rect &area, Color base, bool pressed, float radius)
    {
        if (area.width <= 0.0F || area.height <= 0.0F)
            return;
        canvas.fillRoundedRect(area, radius, outline, outline);
        Rect  body  = area.inset(1.0F);
        Color light = shade(base, pressed ? -10 : 28);
        Color dark  = shade(base, pressed ? -32 : -18);
        canvas.fillRoundedRect(body, std::max(0.0F, radius - 1.0F), pressed ? dark : light, pressed ? light : dark);
        if (!pressed && body.width > 2 * radius) // the glossy edge along the top
            canvas.fillRect({body.x + radius, body.y + 1.0F, body.width - (2 * radius), 1.0F}, {255, 255, 255, 60});
    }

    void sunken(Canvas &canvas, const Rect &area, Color fill, float radius)
    {
        if (area.width <= 0.0F || area.height <= 0.0F)
            return;
        canvas.fillRoundedRect(area, radius, frame, frame);
        canvas.fillRoundedRect(area.inset(1.0F), std::max(0.0F, radius - 1.0F), fill, fill);
    }
}
