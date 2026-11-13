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
#include "supervisor_client.hpp"
#ifdef Q_OS_ANDROID
#include "android_launcher.hpp"
#endif

#include "common/clock.hpp"
#include "common/std_file_system.hpp"
#include "configuration/configuration.hpp"
#include "downloader/curl_transport.hpp"
#include "player_runtime/threaded_runtime.hpp"

#include <QCommandLineParser>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QtWebView>

#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <random>

namespace
{
    std::string utf8(const QString &text)
    {
        return text.toStdString();
    }

    /// The places of garlic-lib, so an existing player keeps its settings, cache and logs (as in the qwidget player).
    /// Android: cache and logs as given by android::storage().
    garlic::core::PlayerRuntimeConfig runtimeConfig(const QString &index_uri, const QString &configuration_file,
                                                    const QString &cache_dir = {}, const QString &log_dir = {})
    {
        garlic::core::PlayerRuntimeConfig config;
        config.ini_path = utf8(QSettings(QSettings::IniFormat, QSettings::UserScope, "SmilControl", "garlic-player").fileName());
#ifdef Q_OS_WIN // the cache location of Windows can be the program directory, which is not writable
        config.cache_dir = utf8(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/cache");
#else
        config.cache_dir = utf8(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
#endif
        config.log_dir     = utf8(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/logs");
        if (!cache_dir.isEmpty())
            config.cache_dir = utf8(cache_dir);
        if (!log_dir.isEmpty())
            config.log_dir = utf8(log_dir);
        config.version     = PLAYER_VERSION;
        config.index_uri   = utf8(index_uri);
        if (!configuration_file.isEmpty())
            config.configuration_file = utf8(QFileInfo(configuration_file).absoluteFilePath());
        config.random_seed = std::random_device{}();
        return config;
    }
}

int main(int argc, char *argv[])
{
    // like garlic-lib: widgets read local files and remote data (desktop: Qt WebView runs on WebEngine)
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--allow-file-access-from-files --disable-web-security");
    QtWebView::initialize(); // before the application
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("garlic-player");
    QGuiApplication::setApplicationVersion(PLAYER_VERSION);

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({
        {{"c", "config"}, "path to a config.xml, applied at every start (e.g. by a launcher)", "config"},
        {{"m", "windows-mode"}, "fullscreen, bigscreen (over all screens) or windowed", "fullscreen|bigscreen|windowed",
#ifdef Q_OS_ANDROID
         "fullscreen"},
#else
         "windowed"},
#endif
        {{"z", "windows-size"}, "size in windows mode, e.g. 980x540", "width x height", "980x540"},
        {{"s", "screen-select"}, "number of the screen to start on", "screen", "0"},
        {"supervisor", "endpoint of the supervisor (garlic-daemon: Unix socket, garlic-service: named pipe)", "endpoint"},
    });
    parser.addPositionalArgument("SMIL_INDEX", "URI or path of the SMIL index; empty: the one in the settings");
    parser.process(app);

    QString index_uri;
    if (!parser.positionalArguments().isEmpty())
    {
        auto valid = garlic::core::configuration::Configuration::validateIndexUri(utf8(parser.positionalArguments().first()));
        if (!valid)
        {
            std::cerr << valid.error() << "\n";
            return 1;
        }
        index_uri = QString::fromStdString(*valid);
    }

    garlic::core::downloader::CurlTransportConfig transport_config;
    QString                                       fallback_index; // when the settings have none

    // supervisor protocol (src/core/docs/supervisor_protocol.md): hello before the core starts, its uuid is a
    // start value; without answer the player starts anyway and the client keeps trying
    std::unique_ptr<player::SupervisorClient> supervisor;
    if (parser.isSet("supervisor"))
    {
        supervisor = std::make_unique<player::SupervisorClient>(parser.value("supervisor"), PLAYER_VERSION);
        supervisor->start(std::chrono::seconds(3));
    }
#ifdef Q_OS_ANDROID
    // launcher 1.x or Philips (as the legacy player): uuid, content URL, restarts, installations, screen
    std::unique_ptr<player::android::Launcher> launcher;
    if (!supervisor)
        launcher = player::android::Launcher::detect();
    player::android::Storage                   storage  = player::android::storage();
    auto config = runtimeConfig(index_uri, parser.value("config"), storage.cache_dir, storage.log_dir);
    transport_config.ca_bundle = player::android::caBundle(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    if (launcher)
    {
        config.supervisor = launcher.get();
        config.uuid       = utf8(launcher->uuid()); // only for a new player: a stored uuid stays
        if (launcher->ownsIdentity() && config.index_uri.empty() && !launcher->contentUrl().isEmpty())
            config.index_uri = utf8(launcher->contentUrl()); // the content URL of the garlic-launcher wins
        else if (!launcher->ownsIdentity())
            fallback_index = launcher->contentUrl(); // Philips: only a default
    }
#else
    auto config = runtimeConfig(index_uri, parser.value("config"));
#endif
    if (supervisor)
    {
        config.supervisor = supervisor.get();
        supervisor->seed(config);
    }

    garlic::core::common::StdFileSystem     fs;
    garlic::core::common::SystemClock       clock;
    garlic::core::downloader::CurlTransport http(transport_config);
    player::Player                         *bridge = nullptr;
    garlic::core::ThreadedRuntime           runtime(fs, clock, http, config,
                                                    [&bridge]
                                                    {
                                                        // core thread: into the UI thread
                                                        QMetaObject::invokeMethod(bridge, [&bridge] { bridge->takeEvents(); },
                                                                                  Qt::QueuedConnection);
                                                    });
    player::Player player(runtime);
    bridge                   = &player; // before start(): the core thread only reads it afterwards
    player::Player::instance = &player; // the QML singleton Player
    if (auto started = runtime.start(); !started)
    {
        std::cerr << "garlic-player cannot start: " << started.error() << "\n";
        return 1;
    }

    // without an index nothing can be played: QML opens the settings dialog first, cancelled ends the player
    std::promise<std::string> stored_uri;
    runtime.post(
        [&](garlic::core::PlayerRuntimeApi &r)
        {
            if (supervisor && supervisor->ready())
                player::SupervisorClient::applyStartValues(r, supervisor->hello());
            std::string uri = r.configuration().indexUri();
            if (uri.empty() && !fallback_index.isEmpty())
            {
                garlic::core::PlayerSettings next = r.playerSettings();
                next.index_uri                    = utf8(fallback_index);
                if (r.changePlayerSettings(next))
                    uri = next.index_uri;
            }
            stored_uri.set_value(uri);
        });
    bool without_index = stored_uri.get_future().get().empty();
    player.setNeedsSettings(without_index);
    if (supervisor)
    {
        // with configuration the supervisor gives name and content URL; locked: no menu, dialogs, shortcuts
        bool configures = supervisor->offers("configuration");
        player.setIdentityLocked(configures);
        player.setNeedsSettings(without_index && !configures);
        player.setLocked(supervisor->locked());
        player.setScreenControl(supervisor.get());
        supervisor->setCoreProbe([&runtime](std::function<void()> done)
                                 { runtime.post([done](garlic::core::PlayerRuntimeApi &) { done(); }); });
        QObject::connect(supervisor.get(), &player::SupervisorClient::lockedChanged, &player, &player::Player::setLocked);
        QObject::connect(supervisor.get(), &player::SupervisorClient::configurationRequested, &player,
                         &player::Player::applyConfiguration);
        QObject::connect(supervisor.get(), &player::SupervisorClient::indexRequested, &player, &player::Player::playIndex);
        QObject::connect(&player, &player::Player::quitRequested, supervisor.get(),
                         [&supervisor] { supervisor->closing("user"); });
        QObject::connect(supervisor.get(), &player::SupervisorClient::quitRequested, &app,
                         [&supervisor]
                         {
                             supervisor->closing("quit");
                             QCoreApplication::quit();
                         });
    }
#ifdef Q_OS_ANDROID
    if (launcher)
    {
        // the garlic-launcher gives name and content URL; without them the player waits for its configuration
        player.setIdentityLocked(launcher->ownsIdentity());
        player.setNeedsSettings(without_index && !launcher->ownsIdentity());
        player.setScreenControl(launcher.get());
        QObject::connect(launcher.get(), &player::android::Launcher::configurationReceived, &player,
                         &player::Player::applyConfiguration);
        QObject::connect(launcher.get(), &player::android::Launcher::indexReceived, &player, &player::Player::playIndex);
        QObject::connect(&player, &player::Player::quitRequested, launcher.get(),
                         [&launcher] { launcher->closedCorrectly(); });
    }
#endif

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("Garlic", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    player.setWindow(window);

    QString  mode   = parser.value("windows-mode");
    QScreen *screen = QGuiApplication::screens().value(parser.value("screen-select").toInt(), QGuiApplication::primaryScreen());
    if (mode == "fullscreen")
        player.showFullScreenOn(screen);
    else if (mode == "bigscreen")
        player.showOverAllScreens();
    else
    {
        QStringList size = parser.value("windows-size").split('x');
        int         w    = size.value(0).toInt();
        int         h    = size.value(1).toInt();
        if (screen != nullptr && window != nullptr)
            window->setPosition(screen->geometry().topLeft());
        player.showWindowed(w > 0 && h > 0 ? QSize(w, h) : QSize(980, 540));
    }

    int result = QGuiApplication::exec();
    runtime.stop(); // before the bridge goes: no wake may reach it any more
    return result;
}
