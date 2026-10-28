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
#include "region.hpp"

#include <QMouseEvent>

namespace player
{
    namespace
    {
        /// backgroundRepeat of the SMIL → CSS (garlic-lib)
        QString cssRepeat(const QString &repeat)
        {
            QString value = repeat.toLower();
            if (value == "repeatx")
                return "repeat-x";
            if (value == "repeaty")
                return "repeat-y";
            if (value == "norepeat")
                return "no-repeat";
            return "repeat";
        }
    }

    void setBackground(QWidget &widget, const QString &color, const QString &image_path, const QString &repeat)
    {
        QString style = "#" + widget.objectName() + " { background-color: " + color + ";";
        if (!image_path.isEmpty())
            style += " background-image: url(\"" + image_path + "\"); background-repeat: " + cssRepeat(repeat) + ";";
        widget.setStyleSheet(style + " }");
    }

    Region::Region(const garlic::core::smil::Region &definition, QWidget *parent)
        : QWidget(parent), region(definition), stack(new QStackedLayout(this))
    {
        setObjectName("region");
        setAttribute(Qt::WA_StyledBackground);
        stack->setContentsMargins(0, 0, 0, 0);
        setBackgroundImage({});
    }

    void Region::place(QSize window)
    {
        setGeometry(qRound(window.width() * region.left), qRound(window.height() * region.top),
                    qRound(window.width() * region.width), qRound(window.height() * region.height));
    }

    void Region::setBackgroundImage(const QString &path)
    {
        setBackground(*this, QString::fromStdString(region.background_color), path,
                      QString::fromStdString(region.background_repeat));
    }

    void Region::addMedia(QWidget *media)
    {
        stack->addWidget(media);
        stack->setCurrentWidget(media);
    }

    void Region::raiseMedia(QWidget *media)
    {
        if (stack->indexOf(media) >= 0)
            stack->setCurrentWidget(media);
    }

    void Region::removeMedia(QWidget *media)
    {
        stack->removeWidget(media);
        media->setParent(nullptr);
    }

    void Region::mouseReleaseEvent(QMouseEvent *event)
    {
        Q_EMIT clicked();
        QWidget::mouseReleaseEvent(event);
    }
}
