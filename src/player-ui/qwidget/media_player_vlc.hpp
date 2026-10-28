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
#ifndef PLAYER_UI_QWIDGET_MEDIA_PLAYER_VLC_HPP
#define PLAYER_UI_QWIDGET_MEDIA_PLAYER_VLC_HPP

#include "media_player.hpp"

struct libvlc_media_player_t;
struct libvlc_event_t;

namespace player
{
    /**
     * libvlc: draws into the native window of a widget (X11, Windows, macOS). On Linux start with --platform xcb,
     * under Wayland libvlc otherwise opens a second window. The events of libvlc come from its own threads and are
     * handed to the UI thread.
     * fit is not supported (like garlic-lib): the video keeps its aspect ratio.
     */
    class VlcMediaPlayer final : public MediaPlayer
    {
        public:
            VlcMediaPlayer();
            ~VlcMediaPlayer() override;

            QWidget *videoWidget() override { return &video; }

            void load(const QString &path) override;
            void play() override;
            void pause() override;
            void resume() override;
            void stop() override;
            void restart() override;
            void setVolume(double volume) override;
            void setAspectRatioMode(Qt::AspectRatioMode) override {}

        private:
            QWidget                video;
            libvlc_media_player_t *player = nullptr;
            int                    volume = 100;

            static void onEvent(const libvlc_event_t *event, void *self);
    };
}

#endif // PLAYER_UI_QWIDGET_MEDIA_PLAYER_VLC_HPP
