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
    namespace
    {
        /**
         * QtAVPlayer hands only these formats to Qt (QAVVideoFrame::operator QVideoFrame); every other one, e.g. 10 bit
         * like yuv420p10le of HEVC Main10, becomes an empty frame: black, and a warning per frame. Those are converted
         * to 8 bit YUV420P by swscale first.
         */
        bool handedOnByQtAvPlayer(AVPixelFormat format)
        {
            switch (format)
            {
                case AV_PIX_FMT_RGB32:
                case AV_PIX_FMT_YUV420P:
                case AV_PIX_FMT_YUV422P:
                case AV_PIX_FMT_NV12:
                case AV_PIX_FMT_VAAPI:
                case AV_PIX_FMT_VDPAU:
                case AV_PIX_FMT_CUDA:
                case AV_PIX_FMT_D3D11:
                case AV_PIX_FMT_VIDEOTOOLBOX:
                case AV_PIX_FMT_VULKAN:
                case AV_PIX_FMT_MEDIACODEC: return true;
                default: return false;
            }
        }
    }

    QtAvMediaPlayer::QtAvMediaPlayer()
    {
        // the frames come from the decoder threads: handed on there, like the examples of QtAVPlayer
        connect(&player, &QAVPlayer::videoFrame, &player,
                [this](const QAVVideoFrame &frame)
                {
                    if (handedOnByQtAvPlayer(frame.format()))
                        video.videoSink()->setVideoFrame(frame);
                    else
                        video.videoSink()->setVideoFrame(frame.convertTo(AV_PIX_FMT_YUV420P));
                },
                Qt::DirectConnection);
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
