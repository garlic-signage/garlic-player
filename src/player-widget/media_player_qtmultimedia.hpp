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
#ifndef PLAYER_WIDGET_MEDIA_PLAYER_QTMULTIMEDIA_HPP
#define PLAYER_WIDGET_MEDIA_PLAYER_QTMULTIMEDIA_HPP

#include "media_player.hpp"

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QVideoWidget>

namespace player
{
    /// Qt Multimedia (FFmpeg backend since Qt 6.5).
    class QtMultimediaPlayer final : public MediaPlayer
    {
        public:
            QtMultimediaPlayer();
            ~QtMultimediaPlayer() override;

            QWidget *videoWidget() override { return &video; }

            void load(const QString &path) override;
            void play() override;
            void pause() override;
            void resume() override;
            void stop() override;
            void restart() override;
            void setVolume(double volume) override;
            void setAspectRatioMode(Qt::AspectRatioMode mode) override;

        private:
            QVideoWidget video;
            QAudioOutput audio;
            QMediaPlayer player;
    };
}

#endif // PLAYER_WIDGET_MEDIA_PLAYER_QTMULTIMEDIA_HPP
