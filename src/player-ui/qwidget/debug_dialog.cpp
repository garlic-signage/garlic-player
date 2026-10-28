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
#include "debug_dialog.hpp"

#include <QApplication>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

#include <future>

namespace player
{
    using garlic::core::DebugInfo;
    using garlic::core::PlayerRuntimeApi;

    namespace
    {
        QString bytes(std::uint64_t value)
        {
            return QLocale().formattedDataSize(static_cast<qint64>(value));
        }

        /// "400 MB free of 1 GB", "-" if the core does not know it
        QString freeOf(const std::optional<std::uint64_t> &free, const std::optional<std::uint64_t> &total)
        {
            if (!free || !total)
                return "-";
            return bytes(*free) + " free of " + bytes(*total);
        }
    }

    DebugDialog::DebugDialog(garlic::core::ThreadedRuntime &r, Playing p, QWidget *parent)
        : QDialog(parent), runtime(r), playing(std::move(p))
    {
        setWindowTitle("Information");
        setMinimumWidth(640);
        for (QLabel *label : {&title, &user_agent, &disk, &memory, &player_memory, &threads, &media})
        {
            label->setTextInteractionFlags(Qt::TextSelectableByMouse);
            label->setWordWrap(true);
        }

        auto *form = new QFormLayout;
        form->addRow("Playlist", &title);
        form->addRow("User agent", &user_agent);
        form->addRow("Disk (cache)", &disk);
        form->addRow("System memory", &memory);
        form->addRow("Player memory", &player_memory);
        form->addRow("Threads", &threads);
        form->addRow("Playing", &media);

        auto *quit  = new QPushButton("Quit player");
        auto *close = new QPushButton("Close");
        close->setDefault(true);
        connect(quit, &QPushButton::clicked, this, [] { QApplication::quit(); });
        connect(close, &QPushButton::clicked, this, &QDialog::reject);
        auto *buttons = new QHBoxLayout;
        buttons->addWidget(quit);
        buttons->addStretch();
        buttons->addWidget(close);

        auto *layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addLayout(buttons);

        connect(&timer, &QTimer::timeout, this, &DebugDialog::refresh);
        timer.start(1000);
        refresh();
    }

    void DebugDialog::refresh()
    {
        std::promise<DebugInfo> answer;
        runtime.post([&answer](PlayerRuntimeApi &api) { answer.set_value(api.debugInfo()); });
        DebugInfo info = answer.get_future().get();

        title.setText(info.index_title.empty() ? "-" : QString::fromStdString(info.index_title));
        user_agent.setText(QString::fromStdString(info.user_agent));
        disk.setText(freeOf(info.disk_free, info.disk_total));
        memory.setText(freeOf(info.memory_free, info.memory_total));
        player_memory.setText(info.memory_used == 0 ? "-"
                                                    : bytes(info.memory_used) + ", max " + bytes(info.memory_max) +
                                                          " at " + QString::fromStdString(info.memory_max_time));
        threads.setText(info.threads == 0 ? "-"
                                          : QString::number(info.threads) + ", max " + QString::number(info.threads_max));
        QStringList now = playing ? playing() : QStringList();
        media.setText(now.isEmpty() ? "-" : now.join('\n'));
    }
}
