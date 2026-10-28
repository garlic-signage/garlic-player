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
#include "image.hpp"

#include <QFile>

namespace player
{
    Image::Image(const garlic::core::smil::Media &s, const garlic::core::smil::Region &r, QString src)
        : Media(s, r, std::move(src))
    {
        label.setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored); // slice: larger than the region, cut
    }

    /// Read from the data, not by the name: sometimes a png is called jpg or the other way round (garlic-lib).
    void Image::start()
    {
        QFile file(source);
        if (!file.open(QIODevice::ReadOnly) || !loaded.loadFromData(file.readAll()))
        {
            Q_EMIT failed("cannot load image " + source);
            return;
        }
        label.setAlignment(alignment(mediaAlign()));
        scale();
    }

    void Image::scale()
    {
        if (loaded.isNull())
            return;
        QSize   size  = label.size();
        QString how   = fit();
        auto    scale = [&](Qt::AspectRatioMode mode) { return loaded.scaled(size, mode, Qt::SmoothTransformation); };

        if (how == "fill")
            label.setPixmap(scale(Qt::IgnoreAspectRatio));
        else if (how == "meet")
            label.setPixmap(scale(Qt::KeepAspectRatio));
        else if (how == "meetbest") // meet, but never larger than the original
            label.setPixmap(loaded.width() > size.width() || loaded.height() > size.height() ? scale(Qt::KeepAspectRatio)
                                                                                              : loaded);
        else if (how == "slice")
            label.setPixmap(scale(Qt::KeepAspectRatioByExpanding));
        else
            label.setPixmap(loaded);
    }

    void Image::Label::resizeEvent(QResizeEvent *event)
    {
        QLabel::resizeEvent(event);
        image.scale();
    }
}
