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
#ifndef PLAYER_UI_QWIDGET_MEDIA_PLAYER_HPP
#define PLAYER_UI_QWIDGET_MEDIA_PLAYER_HPP

#include <QObject>
#include <QStringList>
#include <QWidget>

#include <memory>
#include <optional>

namespace player
{
    /// The video and audio libraries, chosen with --media-backend. Only the ones built in (PLAYER_WITH_*) exist.
    enum class MediaBackend
    {
        QtAvPlayer,   // default
        QtMultimedia,
        Vlc
    };

    QStringList                 mediaBackendNames();
    std::optional<MediaBackend> mediaBackendFromName(const QString &name);

    /**
     * Plays one video or audio file with one of the backends. The backends behave differently at the end, at errors
     * and when stopped; the implementations make them equal for the core:
     *   - exactly one finished() per run (play() or restart() until the end)
     *   - no finished() or failed() after stop()
     */
    class MediaPlayer : public QObject
    {
            Q_OBJECT

        public:
            using QObject::QObject;

            /// Where the video is shown; for audio it is never shown.
            virtual QWidget *videoWidget() = 0;

            virtual void load(const QString &path) = 0;
            virtual void play()                    = 0;
            virtual void pause()                   = 0;
            virtual void resume()                  = 0;
            virtual void stop()                    = 0;
            virtual void restart()                 = 0; // repeatCount: again from the beginning

            /// 0..1
            virtual void setVolume(double volume) = 0;

            /// fit of the SMIL: fill = IgnoreAspectRatio, meet = KeepAspectRatio, slice = KeepAspectRatioByExpanding
            virtual void setAspectRatioMode(Qt::AspectRatioMode mode) = 0;

        Q_SIGNALS:
            void finished();
            void failed(const QString &message);

        protected:
            void began() { running = true; }
            void stopped() { running = false; }

            /// The end of the medium: finished() once per run.
            void ended()
            {
                if (!running)
                    return;
                running = false;
                Q_EMIT finished();
            }

            void error(const QString &message)
            {
                if (!running)
                    return;
                running = false;
                Q_EMIT failed(message);
            }

        private:
            bool running = false;
    };

    std::unique_ptr<MediaPlayer> createMediaPlayer(MediaBackend backend);
}

#endif // PLAYER_UI_QWIDGET_MEDIA_PLAYER_HPP
