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
#ifndef PLAYER_WIDGET_MEDIA_HPP
#define PLAYER_WIDGET_MEDIA_HPP

#include "media_player.hpp"

#include "smil/document.hpp"

#include <QObject>
#include <QString>
#include <QWidget>

#include <memory>

namespace player
{
    /**
     * One medium in one region, from StartMedia to StopMedia. The timing is the business of the core: a medium only
     * shows itself and reports what only the frontend knows (the end of a video, a file that cannot be played).
     *
     * smil and region point into the document of the current NewLayout, which the MainWindow keeps.
     */
    class Media : public QObject
    {
            Q_OBJECT

        public:
            Media(const garlic::core::smil::Media &smil, const garlic::core::smil::Region &region, QString source);

            /// The widget shown in the region; nullptr for audio and commands.
            virtual QWidget *widget() { return nullptr; }

            virtual void start() = 0;
            virtual void stop() {}
            virtual void pause() {}
            virtual void resume() {}
            virtual void repeat() {} // RepeatMedia: video and audio again from the beginning, an image just stays

        Q_SIGNALS:
            void finished();                      // video, audio: the end of the file
            void failed(const QString &message);  // cannot be played: the core goes on

        protected:
            const garlic::core::smil::Media  &smil;
            const garlic::core::smil::Region &region;
            QString                           source; // local path or URL, widgets with their query

            QString fit() const;        // of the medium, else of the region; lower case
            QString mediaAlign() const; // of the medium, else of the region; lower case
            double  volume() const;     // soundLevel of the medium, else of the region: "50%" → 0.5
    };

    /// The class for media.kind; nullptr for kinds that are not shown (prefetch).
    std::unique_ptr<Media> createMedia(const garlic::core::smil::Media &smil, const garlic::core::smil::Region &region,
                                       const QString &source, MediaBackend backend);

    /// mediaAlign → Qt alignment; topLeft if unknown (garlic-lib).
    Qt::Alignment alignment(const QString &media_align);
}

#endif // PLAYER_WIDGET_MEDIA_HPP
