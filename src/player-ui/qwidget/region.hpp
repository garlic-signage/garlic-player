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
#ifndef PLAYER_UI_QWIDGET_REGION_HPP
#define PLAYER_UI_QWIDGET_REGION_HPP

#include "smil/document.hpp"

#include <QStackedLayout>
#include <QWidget>

namespace player
{
    /// Sets background color and image as style sheet, used by regions and the root layout.
    void setBackground(QWidget &widget, const QString &color, const QString &image_path, const QString &repeat);

    /**
     * One <region>: placed relative to the window (0..1), its media stacked, the newest on top. definition points into
     * the document of the current NewLayout.
     */
    class Region final : public QWidget
    {
            Q_OBJECT

        public:
            Region(const garlic::core::smil::Region &definition, QWidget *parent);

            const garlic::core::smil::Region &definition() const { return region; }

            void place(QSize window);
            void setBackgroundImage(const QString &path);

            void addMedia(QWidget *media);    // on top
            void raiseMedia(QWidget *media);  // a resumed medium comes to the top again
            void removeMedia(QWidget *media);

        Q_SIGNALS:
            void clicked();

        protected:
            void mouseReleaseEvent(QMouseEvent *event) override;

        private:
            const garlic::core::smil::Region &region;
            QStackedLayout                   *stack;
    };
}

#endif // PLAYER_UI_QWIDGET_REGION_HPP
