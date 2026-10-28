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
#ifndef PLAYER_WIDGET_SETTINGS_DIALOG_HPP
#define PLAYER_WIDGET_SETTINGS_DIALOG_HPP

#include "player_runtime/threaded_runtime.hpp"

#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

namespace player
{
    /**
     * Ctrl+S and at the start without index (garlic-lib: ConfigDialog): player name, index URI, HTTP server. Reading,
     * checking and storing is done by the core (playerSettings, changePlayerSettings); the dialog only shows the
     * fields and the error text of the core. It waits for the core thread for both.
     */
    class SettingsDialog final : public QDialog
    {
        public:
            SettingsDialog(garlic::core::ThreadedRuntime &runtime, QWidget *parent = nullptr);

            void accept() override;

        private:
            garlic::core::ThreadedRuntime &runtime;
            QLineEdit                      player_name;
            QLineEdit                      index_uri;
            QCheckBox                      httpd_enabled;
            QSpinBox                       httpd_port;
            QLabel                         error;
    };
}

#endif // PLAYER_WIDGET_SETTINGS_DIALOG_HPP
