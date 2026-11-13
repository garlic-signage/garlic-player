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
#ifndef PLAYER_UI_QML_ANDROID_LAUNCHER_HPP
#define PLAYER_UI_QML_ANDROID_LAUNCHER_HPP

#include "player.hpp"

#include "player_runtime/supervisor.hpp"

#include <QObject>
#include <QString>

#include <memory>
#include <string>

namespace player::android
{
    /**
     * The launcher 1.x or the Philips firmware, through the Java class GarlicActivity, which finds it before Qt
     * starts. Supervisor of the core (restart, installation), screen of adapi:blankScreen, and the configuration and
     * index the launcher sends (ConfigReceiver, SmilIndexReceiver). Only on Android.
     *
     * The broadcasts of launcher 1.x give no answer: whether a restart or an installation happened, the core sees at
     * the next start (TaskScheduler::checkPending). No heartbeat.
     */
    class Launcher final : public QObject, public garlic::core::Supervisor, public ScreenControl
    {
            Q_OBJECT

        public:
            /// The launcher of the device with its uuid, content URL and version; nullptr without one.
            static std::unique_ptr<Launcher> detect();
            ~Launcher() override;

            const QString &name() const noexcept { return launcher_name; } // "garlic" or "philips"
            const QString &uuid() const noexcept { return launcher_uuid; }
            const QString &contentUrl() const noexcept { return content_url; }
            const QString &version() const noexcept { return launcher_version; }

            /// garlic-launcher: name and content URL come from it, the settings dialog may not change them.
            bool ownsIdentity() const noexcept { return launcher_name == "garlic"; }

            // Supervisor, called in the core thread
            void reboot(const std::string &task_id) override;
            void installSoftware(const std::string &path) override;

            // ScreenControl
            void screenOff() override;
            void screenOn() override;
            void deepStandby(std::chrono::seconds wake_up) override;

            /// The player ends on purpose (menu, keys): the launcher shall not start it again.
            void closedCorrectly();

            /// From the JNI functions of ConfigReceiver and SmilIndexReceiver (Android main thread).
            static void received(bool configuration, const QString &path);

        Q_SIGNALS:
            void configurationReceived(const QString &path); // a configuration.xml to apply
            void indexReceived(const QString &path);         // a SMIL index to play from now on

        private:
            Launcher() = default;

            QString launcher_name, launcher_uuid, content_url, launcher_version;
    };

    /// Where cache and logs go: /sdcard/garlic-player/{cache,logs} like the legacy player if the storage is permitted
    /// (launcher 1.x installs the downloaded APK from there), otherwise inside the app.
    struct Storage
    {
        QString cache_dir;
        QString log_dir;
        bool    shared = false;
    };
    Storage storage();

    /**
     * The CA certificates of the system as one PEM file for libcurl: Android has no OpenSSL store, its certificates
     * are single files in /apex/com.android.conscrypt/cacerts (Android 14+) or /system/etc/security/cacerts. Written
     * anew at every start into dir, so updates of the system count. Empty if there are none.
     */
    std::string caBundle(const QString &dir);
}

#endif // PLAYER_UI_QML_ANDROID_LAUNCHER_HPP
