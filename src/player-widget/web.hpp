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
#ifndef PLAYER_WIDGET_WEB_HPP
#define PLAYER_WIDGET_WEB_HPP

#include "media.hpp"

#include <QWebEngineView>

namespace player
{
    /// <text>, <ref type="text/html">, widgets (extracted index.html with the query of their params).
    class Web final : public Media
    {
        public:
            Web(const garlic::core::smil::Media &smil, const garlic::core::smil::Region &region, QString source);

            QWidget *widget() override { return &browser; }

            void start() override;
            void stop() override;
            void pause() override { browser.setVisible(false); }
            void resume() override { browser.setVisible(true); }

        private:
            QWebEngineView browser;
    };
}

#endif // PLAYER_WIDGET_WEB_HPP
