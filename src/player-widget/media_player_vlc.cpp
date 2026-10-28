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
#include "media_player_vlc.hpp"

#include <QDir>

#include <vlc/vlc.h>

namespace player
{
    namespace
    {
        /// One libvlc instance for all players (garlic-lib created one per video).
        libvlc_instance_t *instance()
        {
            static libvlc_instance_t *vlc = libvlc_new(0, nullptr);
            return vlc;
        }
    }

    VlcMediaPlayer::VlcMediaPlayer()
    {
        video.setAttribute(Qt::WA_NativeWindow);
        video.setStyleSheet("background-color: black");
        if (instance() == nullptr)
            return;
        player = libvlc_media_player_new(instance());
        if (player == nullptr)
            return;

        auto handle = video.winId();
#if defined(Q_OS_WIN)
        libvlc_media_player_set_hwnd(player, reinterpret_cast<void *>(handle));
#elif defined(Q_OS_MACOS)
        libvlc_media_player_set_nsobject(player, reinterpret_cast<void *>(handle));
#else
        libvlc_media_player_set_xwindow(player, static_cast<std::uint32_t>(handle));
#endif
        libvlc_video_set_mouse_input(player, false); // clicks go to the region
        libvlc_video_set_key_input(player, false);

        libvlc_event_manager_t *events = libvlc_media_player_event_manager(player);
        libvlc_event_attach(events, libvlc_MediaPlayerEndReached, &VlcMediaPlayer::onEvent, this);
        libvlc_event_attach(events, libvlc_MediaPlayerEncounteredError, &VlcMediaPlayer::onEvent, this);
    }

    VlcMediaPlayer::~VlcMediaPlayer()
    {
        stopped();
        if (player == nullptr)
            return;
        libvlc_media_player_stop(player); // waits for the threads of libvlc, so no event comes afterwards
        libvlc_event_manager_t *events = libvlc_media_player_event_manager(player);
        libvlc_event_detach(events, libvlc_MediaPlayerEndReached, &VlcMediaPlayer::onEvent, this);
        libvlc_event_detach(events, libvlc_MediaPlayerEncounteredError, &VlcMediaPlayer::onEvent, this);
        libvlc_media_player_release(player);
    }

    /// In a thread of libvlc: no libvlc call here, only hand over to the UI thread.
    void VlcMediaPlayer::onEvent(const libvlc_event_t *event, void *self)
    {
        auto *me  = static_cast<VlcMediaPlayer *>(self);
        bool  end = event->type == libvlc_MediaPlayerEndReached;
        QMetaObject::invokeMethod(me, [me, end] { end ? me->ended() : me->error("libvlc error"); }, Qt::QueuedConnection);
    }

    void VlcMediaPlayer::load(const QString &path)
    {
        if (player == nullptr)
            return;
        QByteArray       native = QDir::toNativeSeparators(path).toUtf8();
        libvlc_media_t  *media  = path.contains("://") ? libvlc_media_new_location(instance(), path.toUtf8().constData())
                                                       : libvlc_media_new_path(instance(), native.constData());
        libvlc_media_player_set_media(player, media);
        if (media != nullptr)
            libvlc_media_release(media); // the player keeps its own reference
    }

    void VlcMediaPlayer::play()
    {
        began();
        if (player == nullptr || libvlc_media_player_play(player) != 0)
            error("libvlc cannot play");
        else
            libvlc_audio_set_volume(player, volume);
    }

    void VlcMediaPlayer::pause()
    {
        if (player != nullptr)
            libvlc_media_player_set_pause(player, 1);
    }

    void VlcMediaPlayer::resume()
    {
        if (player != nullptr)
            libvlc_media_player_set_pause(player, 0);
    }

    void VlcMediaPlayer::stop()
    {
        stopped();
        if (player != nullptr)
            libvlc_media_player_stop(player);
    }

    /// After the end libvlc is in the state Ended, where seeking does nothing: stop and play again.
    void VlcMediaPlayer::restart()
    {
        stopped();
        if (player != nullptr)
            libvlc_media_player_stop(player);
        play();
    }

    void VlcMediaPlayer::setVolume(double value)
    {
        volume = static_cast<int>(value * 100 + 0.5);
        if (player != nullptr)
            libvlc_audio_set_volume(player, volume);
    }
}
