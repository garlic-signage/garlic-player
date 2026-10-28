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
#include "media_player_qtav.hpp"

#include <QVideoSink>

namespace player
{
    QtAvMediaPlayer::QtAvMediaPlayer()
    {
        // the frames come from the decoder threads: handed on there, like the examples of QtAVPlayer
        connect(&player, &QAVPlayer::videoFrame, &player,
                [this](const QAVVideoFrame &frame) { video.videoSink()->setVideoFrame(frame); }, Qt::DirectConnection);
        connect(&player, &QAVPlayer::audioFrame, &player, [this](const QAVAudioFrame &frame) { audio.play(frame); },
                Qt::DirectConnection);

        connect(&player, &QAVPlayer::mediaStatusChanged, this,
                [this](QAVPlayer::MediaStatus status)
                {
                    if (status == QAVPlayer::EndOfMedia)
                        ended();
                    else if (status == QAVPlayer::InvalidMedia)
                        error("invalid media");
                });
        connect(&player, &QAVPlayer::errorOccurred, this,
                [this](QAVPlayer::Error, const QString &message) { error(message); });
    }

    QtAvMediaPlayer::~QtAvMediaPlayer()
    {
        stopped();
        player.stop();
    }

    void QtAvMediaPlayer::load(const QString &path)
    {
        player.setSource(path);
    }

    void QtAvMediaPlayer::play()
    {
        began();
        player.play();
    }

    void QtAvMediaPlayer::pause()
    {
        player.pause();
    }

    void QtAvMediaPlayer::resume()
    {
        player.play();
    }

    void QtAvMediaPlayer::stop()
    {
        stopped();
        player.stop();
    }

    void QtAvMediaPlayer::restart()
    {
        began();
        player.seek(0);
        player.play();
    }

    void QtAvMediaPlayer::setVolume(double volume)
    {
        audio.setVolume(volume);
    }

    void QtAvMediaPlayer::setAspectRatioMode(Qt::AspectRatioMode mode)
    {
        video.setAspectRatioMode(mode);
    }
}
