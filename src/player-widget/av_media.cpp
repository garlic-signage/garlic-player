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
#include "av_media.hpp"

namespace player
{
    AvMedia::AvMedia(const garlic::core::smil::Media &s, const garlic::core::smil::Region &r, QString src,
                     MediaBackend backend, bool video)
        : Media(s, r, std::move(src)), player(createMediaPlayer(backend)), is_video(video)
    {
        connect(player.get(), &MediaPlayer::finished, this, &Media::finished);
        connect(player.get(), &MediaPlayer::failed, this, &Media::failed);
    }

    QWidget *AvMedia::widget()
    {
        return is_video ? player->videoWidget() : nullptr;
    }

    void AvMedia::start()
    {
        QString how = fit();
        if (how == "fill")
            player->setAspectRatioMode(Qt::IgnoreAspectRatio);
        else if (how == "slice")
            player->setAspectRatioMode(Qt::KeepAspectRatioByExpanding);
        else
            player->setAspectRatioMode(Qt::KeepAspectRatio); // meet, meetBest and the others (garlic-lib)
        player->setVolume(volume());
        player->load(source);
        player->play();
    }
}
