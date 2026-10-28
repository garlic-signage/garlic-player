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
#include "media_player_qtmultimedia.hpp"

#include <QUrl>

namespace player
{
    QtMultimediaPlayer::QtMultimediaPlayer()
    {
        player.setVideoOutput(&video);
        player.setAudioOutput(&audio);
        connect(&player, &QMediaPlayer::mediaStatusChanged, this,
                [this](QMediaPlayer::MediaStatus status)
                {
                    if (status == QMediaPlayer::EndOfMedia)
                        ended();
                    else if (status == QMediaPlayer::InvalidMedia)
                        error("invalid media");
                });
        connect(&player, &QMediaPlayer::errorOccurred, this,
                [this](QMediaPlayer::Error, const QString &message) { error(message); });
    }

    QtMultimediaPlayer::~QtMultimediaPlayer()
    {
        stopped();
        player.stop();
    }

    void QtMultimediaPlayer::load(const QString &path)
    {
        player.setSource(QUrl::fromUserInput(path, {}, QUrl::AssumeLocalFile));
    }

    void QtMultimediaPlayer::play()
    {
        began();
        player.play();
    }

    void QtMultimediaPlayer::pause()
    {
        player.pause();
    }

    void QtMultimediaPlayer::resume()
    {
        player.play();
    }

    void QtMultimediaPlayer::stop()
    {
        stopped();
        player.stop();
    }

    void QtMultimediaPlayer::restart()
    {
        began();
        player.setPosition(0);
        player.play();
    }

    void QtMultimediaPlayer::setVolume(double volume)
    {
        audio.setVolume(static_cast<float>(volume));
    }

    void QtMultimediaPlayer::setAspectRatioMode(Qt::AspectRatioMode mode)
    {
        video.setAspectRatioMode(mode);
    }
}
