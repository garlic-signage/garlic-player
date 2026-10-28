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
#include "settings_dialog.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

#include <future>

namespace player
{
    using garlic::core::PlayerRuntimeApi;
    using garlic::core::PlayerSettings;

    namespace
    {
        /// Runs call in the core thread and waits for its result.
        template <typename Result> Result ask(garlic::core::ThreadedRuntime &runtime,
                                              std::function<Result(PlayerRuntimeApi &)> call)
        {
            std::promise<Result> answer;
            runtime.post([&](PlayerRuntimeApi &r) { answer.set_value(call(r)); });
            return answer.get_future().get();
        }
    }

    SettingsDialog::SettingsDialog(garlic::core::ThreadedRuntime &r, QWidget *parent) : QDialog(parent), runtime(r)
    {
        setWindowTitle("garlic-player settings");
        setMinimumWidth(560);

        httpd_enabled.setText("active");
        httpd_port.setRange(1, 65535);
        connect(&httpd_enabled, &QCheckBox::toggled, &httpd_port, &QSpinBox::setEnabled);
        error.setStyleSheet("color: red");
        error.setWordWrap(true);

        auto *form = new QFormLayout;
        form->addRow("Player name", &player_name);
        form->addRow("Content URL", &index_uri);
        form->addRow("HTTP server", &httpd_enabled);
        form->addRow("Port", &httpd_port);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);

        auto *layout = new QVBoxLayout(this);
        layout->addWidget(&error);
        layout->addLayout(form);
        layout->addWidget(buttons);

        auto current = ask<PlayerSettings>(runtime, [](PlayerRuntimeApi &api) { return api.playerSettings(); });
        player_name.setText(QString::fromStdString(current.player_name));
        index_uri.setText(QString::fromStdString(current.index_uri));
        index_uri.setPlaceholderText("e.g. https://indexes.smil-control.com");
        httpd_enabled.setChecked(current.httpd_enabled);
        httpd_port.setValue(current.httpd_port);
        httpd_port.setEnabled(current.httpd_enabled);
        error.hide();
    }

    /// Closes only when the core has stored the settings; otherwise its error text is shown (garlic-lib).
    void SettingsDialog::accept()
    {
        PlayerSettings next{player_name.text().trimmed().toStdString(), index_uri.text().trimmed().toStdString(),
                            httpd_enabled.isChecked(), static_cast<std::uint16_t>(httpd_port.value())};
        auto changed = ask<std::expected<void, std::string>>(
            runtime, [next](PlayerRuntimeApi &api) { return api.changePlayerSettings(next); });
        if (!changed)
        {
            error.setText(QString::fromStdString(changed.error()));
            error.show();
            return;
        }
        QDialog::accept();
    }
}
