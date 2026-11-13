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
#include "runtime_requests.hpp"

#include "configuration/configuration.hpp"
#include "player_runtime/threaded_runtime.hpp"

#include <QDebug>

namespace player
{
    using garlic::core::PlayerRuntimeApi;

    void applyConfiguration(garlic::core::ThreadedRuntime &runtime, const QString &path)
    {
        runtime.post([file = path.toStdString()](PlayerRuntimeApi &r) { r.applyConfigurationFile(file); });
    }

    void playIndex(garlic::core::ThreadedRuntime &runtime, const QString &uri)
    {
        runtime.post(
            [given = uri.toStdString()](PlayerRuntimeApi &r)
            {
                auto valid = garlic::core::configuration::Configuration::validateIndexUri(given);
                if (!valid)
                {
                    qWarning().noquote() << "index not taken:" << QString::fromStdString(valid.error());
                    return;
                }
                garlic::core::PlayerSettings next = r.playerSettings();
                next.index_uri                    = *valid;
                if (auto changed = r.changePlayerSettings(next); !changed)
                    qWarning().noquote() << "index not taken:" << QString::fromStdString(changed.error());
            });
    }
}
