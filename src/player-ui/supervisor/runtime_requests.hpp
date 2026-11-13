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
#ifndef PLAYER_UI_SUPERVISOR_RUNTIME_REQUESTS_HPP
#define PLAYER_UI_SUPERVISOR_RUNTIME_REQUESTS_HPP

#include <QString>

namespace garlic::core
{
    class ThreadedRuntime;
}

namespace player
{
    /// What a supervisor or launcher asks of the player, into the core thread; the same for every frontend.

    /// A configuration.xml to apply now (applyConfiguration, ConfigReceiver): the index is loaded again.
    void applyConfiguration(garlic::core::ThreadedRuntime &runtime, const QString &path);

    /// A SMIL index to store and play from now on (setIndex, SmilIndexReceiver); garlic-lib: reloadWithNewIndex.
    void playIndex(garlic::core::ThreadedRuntime &runtime, const QString &uri);
}

#endif // PLAYER_UI_SUPERVISOR_RUNTIME_REQUESTS_HPP
