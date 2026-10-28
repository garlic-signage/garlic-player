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
#ifndef PLAYER_WIDGET_IMAGE_HPP
#define PLAYER_WIDGET_IMAGE_HPP

#include "media.hpp"

#include <QLabel>
#include <QPixmap>

namespace player
{
    /// <img>: scaled by fit (fill, meet, meetBest, slice, else the original size), placed by mediaAlign.
    class Image final : public Media
    {
        public:
            Image(const garlic::core::smil::Media &smil, const garlic::core::smil::Region &region, QString source);

            QWidget *widget() override { return &label; }
            void     start() override;

        private:
            /// Scales the image whenever the region changes its size.
            class Label final : public QLabel
            {
                public:
                    explicit Label(Image &owner) : image(owner) {}

                protected:
                    void resizeEvent(QResizeEvent *event) override;

                private:
                    Image &image;
            };

            Label   label{*this};
            QPixmap loaded;

            void scale();
    };
}

#endif // PLAYER_WIDGET_IMAGE_HPP
