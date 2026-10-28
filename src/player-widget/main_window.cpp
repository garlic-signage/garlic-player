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
#include "main_window.hpp"
#include "debug_dialog.hpp"
#include "settings_dialog.hpp"

#include <QApplication>
#include <QDateTime>
#include <QMouseEvent>
#include <QDebug>
#include <QKeyEvent>
#include <QMenu>
#include <QWindow>
#include <QProcess>
#include <QScreen>
#include <QWebEngineProfile>

namespace player
{
    using namespace garlic::core;

    namespace
    {
        template <typename... Functions> struct Overloaded : Functions...
        {
            using Functions::operator()...;
        };

        QString qt(const std::string &text)
        {
            return QString::fromStdString(text);
        }
    }

    MainWindow::MainWindow(ThreadedRuntime &r, MediaBackend b) : runtime(r), backend(b)
    {
        setObjectName("root");
        setAttribute(Qt::WA_StyledBackground);
        setBackground(*this, "black", {}, {});
        setWindowTitle("garlic-player");
        qApp->installEventFilter(this); // clicks and touches anywhere in the window, also on web pages
        hold.setSingleShot(true);
        hold.setInterval(10000);
        connect(&hold, &QTimer::timeout, this, &MainWindow::openMenu);
    }

    MainWindow::~MainWindow()
    {
        clear(); // the media before the regions, which own their widgets for the moment
    }

    void MainWindow::takeEvents()
    {
        for (const CoreEvent &event : runtime.takeEvents())
            handle(event);
    }

    void MainWindow::handle(const CoreEvent &event)
    {
        std::visit(Overloaded{
                       [this](const smil::StartMedia &e) { start(e); },
                       [this](const smil::StopMedia &e) { stop(e.id); },
                       [this](const smil::RepeatMedia &e) { forEach(e.id, &Media::repeat); },
                       [this](const smil::PauseMedia &e) { forEach(e.id, &Media::pause); },
                       [this](const smil::ResumeMedia &e)
                       {
                           forEach(e.id, &Media::resume);
                           if (auto it = instances.find(e.id); it != instances.end())
                               for (auto &[region, media] : it->second.shown)
                                   if (QWidget *widget = media->widget())
                                       region->raiseMedia(widget);
                       },
                       [this](const NewLayout &e) { newLayout(e); },
                       [this](const BackgroundImage &e) { backgroundImage(e); },
                       // no launcher yet: garlic-lib did nothing on the desktop either
                       [](const tasks::InstallSoftware &e) { qWarning() << "install software: no launcher" << qt(e.path); },
                       [](const tasks::Reboot &e) { qWarning() << "reboot: no launcher" << qt(e.task_id); },
                       [](const tasks::ClearCaches &e)
                       {
                           if (e.web)
                               QWebEngineProfile::defaultProfile()->clearHttpCache();
                           qInfo() << "caches cleared, restart";
                           QProcess::startDetached(QApplication::applicationFilePath(), QApplication::arguments().mid(1));
                           QApplication::quit();
                       },
                       [this](const Screenshot &e) { screenshot(e.path); },
                   },
                   event);
    }

    /// A new index: the old media are stopped already (StopMedia come first), so only the regions go.
    void MainWindow::newLayout(const NewLayout &next)
    {
        clear();
        for (Region *region : regions)
            delete region;
        regions.clear();
        layout = next;

        const smil::Head &head = layout.document->head;
        auto              path = [this](const std::string &src) -> QString
        {
            auto found = layout.backgrounds.find(src);
            return found == layout.backgrounds.end() ? QString() : qt(found->second);
        };
        setBackground(*this, qt(head.root_layout.background_color), path(head.root_layout.background_image),
                      qt(head.root_layout.background_repeat));

        for (const smil::Region &definition : head.regions) // sorted by z-index: a later one lies above
        {
            auto *region = new Region(definition, this);
            region->setBackgroundImage(path(definition.background_image));
            region->place(size());
            region->show();
            connect(region, &Region::clicked, this,
                    [this, region]
                    {
                        // the medium on top of the region
                        for (auto &[id, instance] : instances)
                            for (auto &[shown_in, media] : instance.shown)
                                if (shown_in == region && media->widget() != nullptr && media->widget()->isVisible())
                                {
                                    runtime.post([id](PlayerRuntimeApi &r) { r.elementActivated(id); });
                                    return;
                                }
                    });
            regions.push_back(region);
        }
    }

    void MainWindow::backgroundImage(const BackgroundImage &image)
    {
        const smil::Head &head = layout.document->head;
        if (head.root_layout.background_image == image.src)
            setBackground(*this, qt(head.root_layout.background_color), qt(image.path),
                          qt(head.root_layout.background_repeat));
        for (Region *region : regions)
            if (region->definition().background_image == image.src)
                region->setBackgroundImage(qt(image.path));
    }

    void MainWindow::start(const smil::StartMedia &command)
    {
        stop(command.id); // the same instance again: start anew
        Instance &instance = instances[command.id];
        for (std::size_t index : command.regions)
        {
            if (index >= regions.size())
                continue;
            Region *region = regions[index];
            auto    media  = createMedia(*command.media, region->definition(), qt(command.source), backend);
            if (!media)
                continue;

            // only the first region reports: the core expects one end per instance
            if (instance.shown.empty())
            {
                smil::InstanceId id = command.id;
                connect(media.get(), &Media::finished, this,
                        [this, id] { runtime.post([id](PlayerRuntimeApi &r) { r.mediaFinished(id); }); });
                connect(media.get(), &Media::failed, this,
                        [this, id](const QString &message)
                        {
                            qWarning() << "media failed:" << message;
                            runtime.post([id](PlayerRuntimeApi &r) { r.mediaFailed(id); });
                        });
            }
            if (QWidget *widget = media->widget())
                region->addMedia(widget);
            media->start();
            instance.shown.emplace_back(region, std::move(media));
        }
    }

    void MainWindow::stop(smil::InstanceId id)
    {
        auto it = instances.find(id);
        if (it == instances.end())
            return;
        for (auto &[region, media] : it->second.shown)
        {
            media->stop();
            if (QWidget *widget = media->widget())
                region->removeMedia(widget);
        }
        instances.erase(it);
    }

    void MainWindow::forEach(smil::InstanceId id, void (Media::*action)())
    {
        if (auto it = instances.find(id); it != instances.end())
            for (auto &[region, media] : it->second.shown)
                (media.get()->*action)();
    }

    void MainWindow::clear()
    {
        while (!instances.empty())
            stop(instances.begin()->first);
    }

    /// garlic-lib: the whole screen of the window as JPEG with quality 60.
    void MainWindow::screenshot(const std::string &path)
    {
        QScreen *current = screen();
        if (current != nullptr)
        {
            QRect geometry = current->geometry();
            current->grabWindow(0, geometry.x(), geometry.y(), geometry.width(), geometry.height())
                .save(qt(path), "JPG", 60);
        }
        runtime.post([path](PlayerRuntimeApi &r) { r.screenshotTaken(path); }); // also if it failed: the core answers
    }

    void MainWindow::keyPressEvent(QKeyEvent *event)
    {
        // Ctrl+F full screen, Ctrl+B over all screens, Ctrl+S settings, Ctrl+D debug infos, Ctrl+Q quit (garlic-lib);
        // other keys are accessKeys
        if (event->modifiers().testFlag(Qt::ControlModifier))
        {
            switch (event->key())
            {
                case Qt::Key_F:
                    isFullScreen() ? showWindowed(windowed_size) : showFullScreenOn(screen());
                    return;
                case Qt::Key_B: showOverAllScreens(); return;
                case Qt::Key_D: openDebugDialog(); return;
                case Qt::Key_S: openSettingsDialog(); return;
                case Qt::Key_Q: QApplication::quit(); return;
                default: break;
            }
        }
        QString text = event->text().toLower();
        if (event->modifiers() == Qt::NoModifier && !text.isEmpty())
        {
            char key = text.at(0).toLatin1();
            runtime.post([key](PlayerRuntimeApi &r) { r.accessKey(key); });
            return;
        }
        QWidget::keyPressEvent(event);
    }

    /**
     * Ten fast clicks or touches (less than 500 ms apart) or one held for 10 s anywhere in the window open the menu
     * with settings, debug infos and exit; the same in every frontend. Holding is for interactive content, where fast
     * clicks reach the page too. garlic-lib opened the debug dialog after seven fast clicks or on releasing after
     * 10 s. The filter sees an event once per widget it is handed to, so an event counts once by its timestamp.
     */
    bool MainWindow::eventFilter(QObject *watched, QEvent *event)
    {
        if (!isInThisWindow(watched))
            return false;

        bool press   = event->type() == QEvent::MouseButtonPress || event->type() == QEvent::TouchBegin;
        bool release = event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::TouchEnd;
        if (!press && !release)
            return false;
        auto  *pointer   = static_cast<QPointerEvent *>(event);
        auto   timestamp = static_cast<qint64>(pointer->timestamp());
        qint64 now       = QDateTime::currentMSecsSinceEpoch();

        if (press && timestamp != last_press)
        {
            last_press  = timestamp;
            fast_clicks = now - released_at < 500 ? fast_clicks + 1 : 1;
            if (pointer->pointCount() > 0)
                menu_position = pointer->point(0).globalPosition().toPoint();
            if (fast_clicks >= 10)
            {
                fast_clicks = 0;
                hold.stop();
                QMetaObject::invokeMethod(this, &MainWindow::openMenu, Qt::QueuedConnection);
            }
            else
                hold.start(); // opens the menu while it is still held
        }
        else if (release && timestamp != last_release)
        {
            last_release = timestamp;
            released_at  = now;
            hold.stop();
        }
        return false;
    }

    /**
     * A widget of this window, or a native window inside it: QVideoWidget draws into a window of its own, and the
     * clicks on a video go to that QWindow, not to a widget. Menus and dialogs are windows of their own and do not count.
     */
    bool MainWindow::isInThisWindow(QObject *watched) const
    {
        if (auto *widget = qobject_cast<QWidget *>(watched))
            return widget->window() == this;
        if (auto *window = qobject_cast<QWindow *>(watched))
        {
            while (window->parent() != nullptr)
                window = window->parent();
            return window == windowHandle();
        }
        return false;
    }

    void MainWindow::openMenu()
    {
        // the shortcuts are shown right-aligned in the menu; they work through keyPressEvent
        QMenu menu(this);
        auto  item = [&menu](const QString &text, const QKeySequence &keys)
        {
            QAction *action = menu.addAction(text);
            action->setShortcut(keys);
            action->setShortcutVisibleInContextMenu(true);
            action->setShortcutContext(Qt::WidgetShortcut); // only shown, keyPressEvent handles the keys
            return action;
        };
        QAction *settings = item("Settings", QKeySequence("Ctrl+S"));
        QAction *debug    = item("Debug infos", QKeySequence("Ctrl+D"));
        QAction *exit     = item("Exit", QKeySequence("Ctrl+Q"));

        QCursor hidden = cursor();
        setCursor(Qt::ArrowCursor);
        QAction *chosen = menu.exec(menu_position);
        setCursor(hidden);

        if (chosen == settings)
            openSettingsDialog();
        else if (chosen == debug)
            openDebugDialog();
        else if (chosen == exit)
            QApplication::quit();
    }

    void MainWindow::openSettingsDialog()
    {
        QCursor hidden = cursor();
        setCursor(Qt::ArrowCursor);
        SettingsDialog(runtime, this).exec(); // the core stores and loads the index again
        setCursor(hidden);
    }

    void MainWindow::openDebugDialog()
    {
        QCursor hidden = cursor();
        setCursor(Qt::ArrowCursor);
        DebugDialog(runtime, [this] { return playingMedia(); }, this).exec();
        setCursor(hidden);
    }

    QStringList MainWindow::playingMedia() const
    {
        QStringList lines;
        for (const auto &[id, instance] : instances)
            for (const auto &[region, media] : instance.shown)
            {
                const smil::Region &definition = region->definition();
                lines << qt(definition.region_name.empty() ? definition.id : definition.region_name) + ": " +
                             media->label();
            }
        return lines;
    }

    void MainWindow::resizeEvent(QResizeEvent *event)
    {
        QWidget::resizeEvent(event);
        for (Region *region : regions)
            region->place(size());
    }

    void MainWindow::showWindowed(QSize size)
    {
        windowed_size = size;
        setWindowFlags(Qt::Window);
        setCursor(Qt::ArrowCursor);
        showNormal();
        resize(size);
    }

    void MainWindow::showFullScreenOn(QScreen *target)
    {
        if (target != nullptr)
            move(target->geometry().topLeft());
        setCursor(Qt::BlankCursor);
        showFullScreen();
    }

    /// bigscreen of garlic-lib: one frameless window over all screens
    void MainWindow::showOverAllScreens()
    {
        QRect all;
        for (QScreen *each : QGuiApplication::screens())
            all = all.united(each->geometry());
        setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setCursor(Qt::BlankCursor);
        showNormal();
        setGeometry(all);
    }
}
