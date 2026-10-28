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
#include "media_player.hpp"

#ifdef PLAYER_WITH_QTAVPLAYER
    #include "media_player_qtav.hpp"
#endif
#ifdef PLAYER_WITH_QTMULTIMEDIA
    #include "media_player_qtmultimedia.hpp"
#endif
#ifdef PLAYER_WITH_VLC
    #include "media_player_vlc.hpp"
#endif

namespace player
{
    QStringList mediaBackendNames()
    {
        QStringList names;
#ifdef PLAYER_WITH_QTAVPLAYER
        names << "qtavplayer";
#endif
#ifdef PLAYER_WITH_QTMULTIMEDIA
        names << "qtmultimedia";
#endif
#ifdef PLAYER_WITH_VLC
        names << "vlc";
#endif
        return names;
    }

    std::optional<MediaBackend> mediaBackendFromName(const QString &name)
    {
#ifdef PLAYER_WITH_QTAVPLAYER
        if (name == "qtavplayer")
            return MediaBackend::QtAvPlayer;
#endif
#ifdef PLAYER_WITH_QTMULTIMEDIA
        if (name == "qtmultimedia")
            return MediaBackend::QtMultimedia;
#endif
#ifdef PLAYER_WITH_VLC
        if (name == "vlc")
            return MediaBackend::Vlc;
#endif
        return std::nullopt;
    }

    std::unique_ptr<MediaPlayer> createMediaPlayer(MediaBackend backend)
    {
        switch (backend)
        {
#ifdef PLAYER_WITH_QTAVPLAYER
            case MediaBackend::QtAvPlayer: return std::make_unique<QtAvMediaPlayer>();
#endif
#ifdef PLAYER_WITH_QTMULTIMEDIA
            case MediaBackend::QtMultimedia: return std::make_unique<QtMultimediaPlayer>();
#endif
#ifdef PLAYER_WITH_VLC
            case MediaBackend::Vlc: return std::make_unique<VlcMediaPlayer>();
#endif
            default: break;
        }
        return nullptr; // not built in: mediaBackendFromName() does not return it
    }
}
