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
#include "media.hpp"

#include "av_media.hpp"
#include "image.hpp"
#include "web.hpp"

#include <QDebug>

namespace player
{
    using garlic::core::smil::MediaKind;

    namespace
    {
        QString lower(const std::string &text)
        {
            return QString::fromStdString(text).toLower();
        }

        /// <brush color="…">: a colored area.
        class Brush final : public Media
        {
            public:
                using Media::Media;

                QWidget *widget() override { return &area; }

                void start() override
                {
                    area.setAttribute(Qt::WA_StyledBackground);
                    area.setStyleSheet("background-color: " + QString::fromStdString(smil.color));
                }

            private:
                QWidget area;
        };

        /**
         * <ref src="adapi:…">. garlic-lib only knew adapi:blankScreen, which switched the screen off through the
         * launcher of Android; without launcher it did nothing. Other commands could not be played.
         */
        class Command final : public Media
        {
            public:
                using Media::Media;

                void start() override
                {
                    if (source.compare("adapi:blankScreen", Qt::CaseInsensitive) == 0)
                        qInfo() << "blankScreen: no launcher, the screen stays on";
                    else
                        QMetaObject::invokeMethod(this, [this] { Q_EMIT failed("unknown command " + source); },
                                                  Qt::QueuedConnection);
                }
        };
    }

    Media::Media(const garlic::core::smil::Media &s, const garlic::core::smil::Region &r, QString src)
        : smil(s), region(r), source(std::move(src))
    {
    }

    QString Media::fit() const
    {
        return lower(smil.fit.empty() ? region.fit : smil.fit);
    }

    QString Media::mediaAlign() const
    {
        return lower(smil.media_align.empty() ? region.media_align : smil.media_align);
    }

    double Media::volume() const
    {
        QString level = QString::fromStdString(smil.sound_level.empty() ? region.sound_level : smil.sound_level);
        if (!level.endsWith('%'))
            return 1;
        bool   ok      = false;
        double percent = level.chopped(1).toDouble(&ok);
        return ok ? std::clamp(percent / 100, 0.0, 1.0) : 1;
    }

    std::unique_ptr<Media> createMedia(const garlic::core::smil::Media &smil, const garlic::core::smil::Region &region,
                                       const QString &source, MediaBackend backend)
    {
        switch (smil.kind)
        {
            case MediaKind::Image: return std::make_unique<Image>(smil, region, source);
            case MediaKind::Video: return std::make_unique<AvMedia>(smil, region, source, backend, true);
            case MediaKind::Audio: return std::make_unique<AvMedia>(smil, region, source, backend, false);
            case MediaKind::Web:
            case MediaKind::Widget: return std::make_unique<Web>(smil, region, source);
            case MediaKind::Brush: return std::make_unique<Brush>(smil, region, source);
            case MediaKind::Command: return std::make_unique<Command>(smil, region, source);
            case MediaKind::Prefetch: return nullptr;
        }
        return nullptr;
    }

    Qt::Alignment alignment(const QString &media_align)
    {
        if (media_align == "center")
            return Qt::AlignCenter;
        if (media_align == "topmid")
            return Qt::AlignTop | Qt::AlignHCenter;
        if (media_align == "topright")
            return Qt::AlignTop | Qt::AlignRight;
        if (media_align == "midleft")
            return Qt::AlignVCenter | Qt::AlignLeft;
        if (media_align == "midright")
            return Qt::AlignVCenter | Qt::AlignRight;
        if (media_align == "bottomleft")
            return Qt::AlignBottom | Qt::AlignLeft;
        if (media_align == "bottommid")
            return Qt::AlignBottom | Qt::AlignHCenter;
        if (media_align == "bottomright")
            return Qt::AlignBottom | Qt::AlignRight;
        return Qt::AlignTop | Qt::AlignLeft;
    }
}
