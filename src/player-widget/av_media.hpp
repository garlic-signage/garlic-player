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
#ifndef PLAYER_WIDGET_AV_MEDIA_HPP
#define PLAYER_WIDGET_AV_MEDIA_HPP

#include "media.hpp"

namespace player
{
    /// <video> and <audio> with the MediaPlayer of the chosen backend. Audio shows no widget.
    class AvMedia final : public Media
    {
        public:
            AvMedia(const garlic::core::smil::Media &smil, const garlic::core::smil::Region &region, QString source,
                    MediaBackend backend, bool video);

            QWidget *widget() override;

            void start() override;
            void stop() override { player->stop(); }
            void pause() override { player->pause(); }
            void resume() override { player->resume(); }
            void repeat() override { player->restart(); }

        private:
            std::unique_ptr<MediaPlayer> player;
            bool                         is_video;
    };
}

#endif // PLAYER_WIDGET_AV_MEDIA_HPP
