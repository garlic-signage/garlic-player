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
#ifndef PLAYER_UI_QML_PLAYER_HPP
#define PLAYER_UI_QML_PLAYER_HPP

#include "region_model.hpp"

#include "player_runtime/threaded_runtime.hpp"

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QPoint>
#include <QSize>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

#include <map>
#include <vector>

class QQuickWindow;
class QScreen;

namespace player
{
    /**
     * The bridge between the core and QML, the singleton "Player" of the QML module Garlic. It takes the events of the ThreadedRuntime
     * and turns them into the region and media models QML binds to; QML reports back what only it knows (end of a
     * video, a click, a key) through its invokable methods. C++ never calls into QML.
     *
     * The same behaviour as the qwidget player: settings, debug infos, the menu (ten fast clicks or holding 10 s),
     * shortcuts, screenshot, window modes.
     */
    class Player final : public QObject
    {
            Q_OBJECT
            QML_ELEMENT
            QML_SINGLETON
            Q_PROPERTY(QObject *regions READ regions CONSTANT)
            Q_PROPERTY(QString rootColor READ rootColor NOTIFY layoutChanged)
            Q_PROPERTY(QString rootImage READ rootImage NOTIFY layoutChanged)
            Q_PROPERTY(QString rootRepeat READ rootRepeat NOTIFY layoutChanged)
            Q_PROPERTY(bool needsSettings READ needsSettings CONSTANT)

        public:
            explicit Player(garlic::core::ThreadedRuntime &runtime);

            /// The one instance made by main(), handed to QML (owned by C++).
            static Player *create(QQmlEngine *, QJSEngine *engine);
            static inline Player *instance = nullptr;

            QObject *regions() { return &region_model; }
            QString  rootColor() const { return root_color; }
            QString  rootImage() const { return root_image; }
            QString  rootRepeat() const { return root_repeat; }
            bool     needsSettings() const noexcept { return needs_settings; }
            void     setNeedsSettings(bool needed) { needs_settings = needed; } // before QML is loaded

            void setWindow(QQuickWindow *window);

            /// Called through the wake of the ThreadedRuntime (queued into the UI thread).
            void takeEvents();

            // --- from QML
            Q_INVOKABLE void mediaFinished(int instance);
            Q_INVOKABLE void mediaFailed(int instance, const QString &message);
            Q_INVOKABLE void activated(int instance);
            Q_INVOKABLE void accessKey(const QString &text);

            Q_INVOKABLE QVariantMap settings();
            Q_INVOKABLE QString     changeSettings(const QVariantMap &settings); // empty or the error text of the core
            Q_INVOKABLE QVariantMap debugInfo();
            Q_INVOKABLE QStringList playingMedia() const;

            Q_INVOKABLE void showCursor(bool shown); // for menus and dialogs; hidden again in full screen
            Q_INVOKABLE void toggleFullScreen();
            Q_INVOKABLE void showOverAllScreens();
            void             showWindowed(QSize size);
            void             showFullScreenOn(QScreen *screen);

        Q_SIGNALS:
            void layoutChanged();
            void menuRequested(QPoint position); // ten fast clicks or one held for 10 s

        protected:
            bool eventFilter(QObject *watched, QEvent *event) override;

        private:
            garlic::core::ThreadedRuntime &runtime;
            bool                           needs_settings = false;
            QQuickWindow                  *window         = nullptr;
            bool                           cursor_hidden  = false;
            QSize                          windowed_size{980, 540};

            garlic::core::NewLayout layout; // the document is kept until the next one
            RegionModel             region_model;
            QString                 root_color = "black", root_image, root_repeat = "repeat";
            std::map<int, std::vector<int>> instances; // instance → rows of the region model it is shown in

            // the menu gesture, like the qwidget player
            QTimer hold;
            qint64 last_press = -1, last_release = -1, released_at = 0;
            int    fast_clicks = 0;
            QPoint menu_position;

            void handle(const garlic::core::CoreEvent &event);
            void newLayout(const garlic::core::NewLayout &next);
            void backgroundImage(const garlic::core::BackgroundImage &image);
            void start(const garlic::core::smil::StartMedia &command);
            void stop(int instance);
            void forEach(int instance, void (MediaModel::*action)(int));
            void screenshot(const std::string &path);
    };
}

#endif // PLAYER_UI_QML_PLAYER_HPP
