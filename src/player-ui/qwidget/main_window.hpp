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
#ifndef PLAYER_UI_QWIDGET_MAIN_WINDOW_HPP
#define PLAYER_UI_QWIDGET_MAIN_WINDOW_HPP

#include "media.hpp"
#include "region.hpp"

#include "player_runtime/threaded_runtime.hpp"

#include <QTimer>
#include <QWidget>

#include <map>
#include <memory>
#include <vector>

namespace player
{
    /**
     * The window of the player: takes the events of the core, builds the regions of a NewLayout and shows the media
     * in them. What the frontend learns (end of a video, a click, a key) goes back to the core by post().
     */
    class MainWindow final : public QWidget
    {
            Q_OBJECT

        public:
            MainWindow(garlic::core::ThreadedRuntime &runtime, MediaBackend backend);
            ~MainWindow() override;

            /// Called through the wake of the ThreadedRuntime (queued into the UI thread).
            void takeEvents();

            void showWindowed(QSize size);
            void showFullScreenOn(QScreen *screen);
            void showOverAllScreens();

            /// The media that play now, "region: title or src", for the debug dialog.
            QStringList playingMedia() const;

        protected:
            bool eventFilter(QObject *watched, QEvent *event) override;
            void keyPressEvent(QKeyEvent *event) override;
            void resizeEvent(QResizeEvent *event) override;

        private:
            /// One StartMedia: the medium in every region with its name.
            struct Instance
            {
                std::vector<std::pair<Region *, std::unique_ptr<Media>>> shown;
            };

            garlic::core::ThreadedRuntime &runtime;
            MediaBackend                   backend;
            QSize                          windowed_size{980, 540};

            garlic::core::NewLayout                                layout;  // the document is kept until the next one
            std::vector<Region *>                                   regions; // children of this window, by z-index
            std::map<garlic::core::smil::InstanceId, Instance>      instances;

            // ten fast clicks/touches or one held for 10 s open the menu (also for the review of the App Store)
            QTimer hold;
            qint64 last_press = -1, last_release = -1; // timestamps of the events: each is counted once
            qint64 released_at = 0;                    // ms since epoch
            int    fast_clicks = 0;
            QPoint menu_position;

            void handle(const garlic::core::CoreEvent &event);
            void newLayout(const garlic::core::NewLayout &next);
            void backgroundImage(const garlic::core::BackgroundImage &image);
            void start(const garlic::core::smil::StartMedia &command);
            void stop(garlic::core::smil::InstanceId id);
            void forEach(garlic::core::smil::InstanceId id, void (Media::*action)());
            void screenshot(const std::string &path);
            void clear();
            bool isInThisWindow(QObject *watched) const;
            void openMenu();
            void openSettingsDialog();
            void openDebugDialog();
    };
}

#endif // PLAYER_UI_QWIDGET_MAIN_WINDOW_HPP
