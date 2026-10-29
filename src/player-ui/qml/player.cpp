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
#include "player.hpp"

#include <QCursor>
#include <QDateTime>
#include <QDebug>
#include <QGuiApplication>
#include <QLocale>
#include <QPointerEvent>
#include <QProcess>
#include <QQuickWindow>
#include <QScreen>
#include <QUrl>

#include <future>

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

        /// A local path or URL of the core as URL for QML (garlic-lib: file:// for local files)
        QString toUrl(const std::string &path)
        {
            if (path.empty())
                return {};
            return QUrl::fromUserInput(qt(path), {}, QUrl::AssumeLocalFile).toString();
        }

        QString kindName(smil::MediaKind kind)
        {
            switch (kind)
            {
                case smil::MediaKind::Image: return "image";
                case smil::MediaKind::Video: return "video";
                case smil::MediaKind::Audio: return "audio";
                case smil::MediaKind::Web:
                case smil::MediaKind::Widget: return "web";
                case smil::MediaKind::Brush: return "brush";
                case smil::MediaKind::Command: return "command";
                case smil::MediaKind::Prefetch: return {};
            }
            return {};
        }

        /// soundLevel "50%" → 0.5, else 1 (like the qwidget player)
        double volumeOf(const std::string &sound_level)
        {
            QString level = qt(sound_level);
            if (!level.endsWith('%'))
                return 1;
            bool   ok      = false;
            double percent = level.chopped(1).toDouble(&ok);
            return ok ? std::clamp(percent / 100, 0.0, 1.0) : 1;
        }

        /// Runs call in the core thread and waits for its result.
        template <typename Result>
        Result ask(ThreadedRuntime &runtime, std::function<Result(PlayerRuntimeApi &)> call)
        {
            std::promise<Result> answer;
            runtime.post([&](PlayerRuntimeApi &api) { answer.set_value(call(api)); });
            return answer.get_future().get();
        }

        QString bytes(std::uint64_t value)
        {
            return QLocale().formattedDataSize(static_cast<qint64>(value));
        }

        QString freeOf(const std::optional<std::uint64_t> &free, const std::optional<std::uint64_t> &total)
        {
            if (!free || !total)
                return "-";
            return bytes(*free) + " free of " + bytes(*total);
        }
    }

    Player *Player::create(QQmlEngine *, QJSEngine *engine)
    {
        engine->setObjectOwnership(instance, QJSEngine::CppOwnership);
        return instance;
    }

    Player::Player(ThreadedRuntime &r) : runtime(r)
    {
        hold.setSingleShot(true);
        hold.setInterval(10000);
        connect(&hold, &QTimer::timeout, this, [this] { Q_EMIT menuRequested(menu_position); });
    }

    void Player::setWindow(QQuickWindow *w)
    {
        window = w;
        if (window != nullptr)
            window->installEventFilter(this);
    }

    void Player::takeEvents()
    {
        for (const CoreEvent &event : runtime.takeEvents())
            handle(event);
    }

    void Player::handle(const CoreEvent &event)
    {
        std::visit(Overloaded{
                       [this](const smil::StartMedia &e) { start(e); },
                       [this](const smil::StopMedia &e) { stop(static_cast<int>(e.id)); },
                       [this](const smil::RepeatMedia &e) { forEach(static_cast<int>(e.id), &MediaModel::repeat); },
                       [this](const smil::PauseMedia &e)
                       {
                           for (int row : instances[static_cast<int>(e.id)])
                               region_model.media(row)->setPaused(static_cast<int>(e.id), true);
                       },
                       [this](const smil::ResumeMedia &e)
                       {
                           for (int row : instances[static_cast<int>(e.id)])
                           {
                               region_model.media(row)->setPaused(static_cast<int>(e.id), false);
                               region_model.media(row)->raise(static_cast<int>(e.id));
                           }
                       },
                       [this](const NewLayout &e) { newLayout(e); },
                       [this](const BackgroundImage &e) { backgroundImage(e); },
                       // no launcher yet: garlic-lib did nothing on the desktop either
                       [](const tasks::InstallSoftware &e) { qWarning() << "install software: no launcher" << qt(e.path); },
                       [](const tasks::Reboot &e) { qWarning() << "reboot: no launcher" << qt(e.task_id); },
                       [](const tasks::ClearCaches &)
                       {
                           // Qt WebView has no cache API; the system WebView keeps its cache
                           qInfo() << "caches cleared, restart";
                           QProcess::startDetached(QGuiApplication::applicationFilePath(),
                                                   QGuiApplication::arguments().mid(1));
                           QGuiApplication::quit();
                       },
                       [this](const Screenshot &e) { screenshot(e.path); },
                   },
                   event);
    }

    /// A new index: the old media are stopped already (StopMedia come first), so only the regions go.
    void Player::newLayout(const NewLayout &next)
    {
        instances.clear();
        layout                 = next;
        const smil::Head &head = layout.document->head;
        auto              url  = [this](const std::string &src) -> QString
        {
            auto found = layout.backgrounds.find(src);
            return found == layout.backgrounds.end() ? QString() : toUrl(found->second);
        };
        root_color  = qt(head.root_layout.background_color);
        root_image  = url(head.root_layout.background_image);
        root_repeat = qt(head.root_layout.background_repeat).toLower();
        region_model.reset(head.regions);
        for (int row = 0; row < region_model.count(); ++row)
            region_model.setBackgroundImage(row, url(region_model.definition(row).background_image));
        Q_EMIT layoutChanged();
    }

    void Player::backgroundImage(const BackgroundImage &image)
    {
        if (!layout.document)
            return;
        if (layout.document->head.root_layout.background_image == image.src)
        {
            root_image = toUrl(image.path);
            Q_EMIT layoutChanged();
        }
        for (int row = 0; row < region_model.count(); ++row)
            if (region_model.definition(row).background_image == image.src)
                region_model.setBackgroundImage(row, toUrl(image.path));
    }

    void Player::start(const smil::StartMedia &command)
    {
        int id = static_cast<int>(command.id);
        stop(id); // the same instance again: start anew
        const smil::Media &media = *command.media;
        QString            kind  = kindName(media.kind);
        if (kind.isEmpty())
            return;
        if (kind == "command") // garlic-lib only knew adapi:blankScreen, which needs the launcher
        {
            if (qt(command.source).compare("adapi:blankScreen", Qt::CaseInsensitive) == 0)
                qInfo() << "blankScreen: no launcher, the screen stays on";
            else
                mediaFailed(id, "unknown command " + qt(command.source));
            return;
        }

        std::vector<int> &rows = instances[id];
        for (std::size_t index : command.regions)
        {
            int row = static_cast<int>(index);
            if (row >= region_model.count())
                continue;
            const smil::Region &region = region_model.definition(row);
            ShownMedia          shown;
            shown.instance = id;
            shown.kind     = kind;
            shown.source   = toUrl(command.source);
            shown.fit      = qt(media.fit.empty() ? region.fit : media.fit).toLower();
            shown.align    = qt(media.media_align.empty() ? region.media_align : media.media_align).toLower();
            shown.volume   = volumeOf(media.sound_level.empty() ? region.sound_level : media.sound_level);
            shown.color    = qt(media.color);
            shown.label    = qt(media.title.empty() ? media.src : media.title);
            shown.primary  = rows.empty(); // the core expects one end per instance
            region_model.media(row)->add(std::move(shown));
            rows.push_back(row);
        }
    }

    void Player::stop(int instance)
    {
        auto it = instances.find(instance);
        if (it == instances.end())
            return;
        for (int row : it->second)
            region_model.media(row)->remove(instance);
        instances.erase(it);
    }

    void Player::forEach(int instance, void (MediaModel::*action)(int))
    {
        for (int row : instances[instance])
            (region_model.media(row)->*action)(instance);
    }

    // ------------------------------------------------------------------ from QML

    void Player::mediaFinished(int instance)
    {
        auto id = static_cast<smil::InstanceId>(instance);
        runtime.post([id](PlayerRuntimeApi &r) { r.mediaFinished(id); });
    }

    void Player::mediaFailed(int instance, const QString &message)
    {
        qWarning() << "media failed:" << message;
        auto id = static_cast<smil::InstanceId>(instance);
        runtime.post([id](PlayerRuntimeApi &r) { r.mediaFailed(id); });
    }

    void Player::activated(int instance)
    {
        auto id = static_cast<smil::InstanceId>(instance);
        runtime.post([id](PlayerRuntimeApi &r) { r.elementActivated(id); });
    }

    void Player::accessKey(const QString &text)
    {
        if (text.isEmpty())
            return;
        char key = text.at(0).toLower().toLatin1();
        runtime.post([key](PlayerRuntimeApi &r) { r.accessKey(key); });
    }

    QVariantMap Player::settings()
    {
        auto current = ask<PlayerSettings>(runtime, [](PlayerRuntimeApi &api) { return api.playerSettings(); });
        return {{"playerName", qt(current.player_name)},
                {"indexUri", qt(current.index_uri)},
                {"httpdEnabled", current.httpd_enabled},
                {"httpdPort", current.httpd_port}};
    }

    QString Player::changeSettings(const QVariantMap &values)
    {
        PlayerSettings next{values.value("playerName").toString().trimmed().toStdString(),
                            values.value("indexUri").toString().trimmed().toStdString(),
                            values.value("httpdEnabled").toBool(),
                            static_cast<std::uint16_t>(values.value("httpdPort").toUInt())};
        auto changed = ask<std::expected<void, std::string>>(
            runtime, [next](PlayerRuntimeApi &api) { return api.changePlayerSettings(next); });
        return changed ? QString() : qt(changed.error());
    }

    QVariantMap Player::debugInfo()
    {
        DebugInfo info = ask<DebugInfo>(runtime, [](PlayerRuntimeApi &api) { return api.debugInfo(); });
        return {{"title", info.index_title.empty() ? QString("-") : qt(info.index_title)},
                {"userAgent", qt(info.user_agent)},
                {"disk", freeOf(info.disk_free, info.disk_total)},
                {"memory", freeOf(info.memory_free, info.memory_total)},
                {"playerMemory", info.memory_used == 0 ? QString("-")
                                                       : bytes(info.memory_used) + ", max " + bytes(info.memory_max) +
                                                             " at " + qt(info.memory_max_time)},
                {"threads", info.threads == 0 ? QString("-")
                                              : QString::number(info.threads) + ", max " + QString::number(info.threads_max)}};
    }

    QStringList Player::playingMedia() const
    {
        QStringList lines;
        for (int row = 0; row < region_model.count(); ++row)
        {
            const smil::Region &definition = region_model.definition(row);
            QString name = qt(definition.region_name.empty() ? definition.id : definition.region_name);
            for (const ShownMedia &media : region_model.media(row)->media())
                lines << name + ": " + media.label;
        }
        return lines;
    }

    // ------------------------------------------------------------------ window

    void Player::showCursor(bool shown)
    {
        if (window != nullptr)
            window->setCursor(shown || !cursor_hidden ? Qt::ArrowCursor : Qt::BlankCursor);
    }

    void Player::toggleFullScreen()
    {
        if (window == nullptr)
            return;
        if (window->visibility() == QWindow::FullScreen)
            showWindowed(windowed_size);
        else
            showFullScreenOn(window->screen());
    }

    void Player::showWindowed(QSize size)
    {
        if (window == nullptr)
            return;
        windowed_size = size;
        cursor_hidden = false;
        window->setFlags(Qt::Window);
        window->setCursor(Qt::ArrowCursor);
        window->showNormal();
        window->resize(size);
    }

    void Player::showFullScreenOn(QScreen *screen)
    {
        if (window == nullptr)
            return;
        if (screen != nullptr)
            window->setPosition(screen->geometry().topLeft());
        cursor_hidden = true;
        window->setCursor(Qt::BlankCursor);
        window->showFullScreen();
    }

    /// bigscreen of garlic-lib: one frameless window over all screens
    void Player::showOverAllScreens()
    {
        if (window == nullptr)
            return;
        QRect all;
        for (QScreen *each : QGuiApplication::screens())
            all = all.united(each->geometry());
        cursor_hidden = true;
        window->setFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        window->setCursor(Qt::BlankCursor);
        window->showNormal();
        window->setGeometry(all);
    }

    /// garlic-lib: the whole screen of the window as JPEG with quality 60.
    void Player::screenshot(const std::string &path)
    {
        QScreen *current = window != nullptr ? window->screen() : QGuiApplication::primaryScreen();
        if (current != nullptr)
        {
            QRect geometry = current->geometry();
            current->grabWindow(0, geometry.x(), geometry.y(), geometry.width(), geometry.height())
                .save(qt(path), "JPG", 60);
        }
        runtime.post([path](PlayerRuntimeApi &r) { r.screenshotTaken(path); }); // also if it failed: the core answers
    }

    /**
     * Ten fast clicks or touches (less than 500 ms apart) or one held for 10 s anywhere in the window open the menu,
     * like in the qwidget player. All input of a QML window goes through its QQuickWindow, so watching it is enough;
     * an event counts once by its timestamp.
     */
    bool Player::eventFilter(QObject *watched, QEvent *event)
    {
        if (watched != window)
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
                menu_position = pointer->point(0).position().toPoint(); // in the window
            if (fast_clicks >= 10)
            {
                fast_clicks = 0;
                hold.stop();
                QMetaObject::invokeMethod(this, [this] { Q_EMIT menuRequested(menu_position); }, Qt::QueuedConnection);
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
}
