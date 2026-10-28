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
#include "media_player.hpp"
#include "settings_dialog.hpp"

#include "common/clock.hpp"
#include "common/std_file_system.hpp"
#include "configuration/configuration.hpp"
#include "downloader/curl_transport.hpp"
#include "player_runtime/threaded_runtime.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>

#include <future>
#include <iostream>
#include <random>

namespace
{
    std::string utf8(const QString &text)
    {
        return text.toStdString();
    }

    /// The places of garlic-lib, so an existing player keeps its settings, cache and logs.
    garlic::core::PlayerRuntimeConfig runtimeConfig(const QString &index_uri, const QString &configuration_file)
    {
        garlic::core::PlayerRuntimeConfig config;
        config.ini_path = utf8(QSettings(QSettings::IniFormat, QSettings::UserScope, "SmilControl", "garlic-player").fileName());
#ifdef Q_OS_WIN // the cache location of Windows can be the program directory, which is not writable
        config.cache_dir = utf8(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/cache");
#else
        config.cache_dir = utf8(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
#endif
        config.log_dir     = utf8(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/logs");
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
    // like garlic-lib: widgets read local files and remote data
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--allow-file-access-from-files --disable-web-security");
    QApplication app(argc, argv);
    QApplication::setApplicationName("garlic-player");
    QApplication::setApplicationVersion(PLAYER_VERSION);

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({
        {{"c", "config"}, "path to a config.xml, applied at every start (e.g. by a launcher)", "config"},
        {{"m", "windows-mode"}, "fullscreen, bigscreen (over all screens) or windowed", "fullscreen|bigscreen|windowed",
         "windowed"},
        {{"z", "windows-size"}, "size in windows mode, e.g. 980x540", "width x height", "980x540"},
        {{"s", "screen-select"}, "number of the screen to start on", "screen", "0"},
        {"media-backend", "video and audio: " + player::mediaBackendNames().join(", "), "backend", "qtavplayer"},
    });
    parser.addPositionalArgument("SMIL_INDEX", "URI or path of the SMIL index; empty: the one in the settings");
    parser.process(app);

    auto backend = player::mediaBackendFromName(parser.value("media-backend"));
    if (!backend)
    {
        std::cerr << "unknown media backend " << utf8(parser.value("media-backend"))
                  << ", built in: " << utf8(player::mediaBackendNames().join(", ")) << "\n";
        return 1;
    }

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

    garlic::core::common::StdFileSystem      fs;
    garlic::core::common::SystemClock        clock;
    garlic::core::downloader::CurlTransport  http;
    player::MainWindow                      *window = nullptr;
    garlic::core::ThreadedRuntime            runtime(fs, clock, http, runtimeConfig(index_uri, parser.value("config")), [&window]
    {
        // core thread: into the UI thread
        QMetaObject::invokeMethod(window, [&window] { window->takeEvents(); }, Qt::QueuedConnection);
    });

    player::MainWindow main_window(runtime, *backend);
    window = &main_window; // before start(): the core thread only reads it afterwards
    if (auto started = runtime.start(); !started)
    {
        std::cerr << "garlic-player cannot start: " << started.error() << "\n";
        return 1;
    }

    // without an index nothing can be played: the settings dialog first, cancelled ends the player (garlic-lib)
    std::promise<std::string> stored_uri;
    runtime.post([&](garlic::core::PlayerRuntimeApi &r) { stored_uri.set_value(r.configuration().indexUri()); });
    if (stored_uri.get_future().get().empty() && player::SettingsDialog(runtime).exec() != QDialog::Accepted)
        return 1;

    QString mode = parser.value("windows-mode");
    QScreen *screen = QGuiApplication::screens().value(parser.value("screen-select").toInt(), QGuiApplication::primaryScreen());
    if (mode == "fullscreen")
        main_window.showFullScreenOn(screen);
    else if (mode == "bigscreen")
        main_window.showOverAllScreens();
    else
    {
        QStringList size = parser.value("windows-size").split('x');
        int         w    = size.value(0).toInt();
        int         h    = size.value(1).toInt();
        if (screen != nullptr)
            main_window.move(screen->geometry().topLeft());
        main_window.showWindowed(w > 0 && h > 0 ? QSize(w, h) : QSize(980, 540));
    }

    int result = QApplication::exec();
    runtime.stop(); // before the window goes: no wake may reach it any more
    return result;
}
