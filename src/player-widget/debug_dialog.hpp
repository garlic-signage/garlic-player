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
#ifndef PLAYER_WIDGET_DEBUG_DIALOG_HPP
#define PLAYER_WIDGET_DEBUG_DIALOG_HPP

#include "player_runtime/threaded_runtime.hpp"

#include <QDialog>
#include <QLabel>
#include <QStringList>
#include <QTimer>

#include <functional>

namespace player
{
    /**
     * Ctrl+D, six fast clicks or touches, or one held for 10 s (garlic-lib: DebugInfos). Every second the resources
     * from the core (debugInfo) and the media that play now, one line per region, from the frontend.
     */
    class DebugDialog final : public QDialog
    {
        public:
            using Playing = std::function<QStringList()>; // "region: title or source"

            DebugDialog(garlic::core::ThreadedRuntime &runtime, Playing playing, QWidget *parent = nullptr);

        private:
            garlic::core::ThreadedRuntime &runtime;
            Playing                        playing;
            QTimer                         timer;
            QLabel                         title, user_agent, disk, memory, player_memory, threads, media;

            void refresh();
    };
}

#endif // PLAYER_WIDGET_DEBUG_DIALOG_HPP
