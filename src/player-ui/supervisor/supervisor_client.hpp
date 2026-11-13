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
#ifndef PLAYER_UI_SUPERVISOR_SUPERVISOR_CLIENT_HPP
#define PLAYER_UI_SUPERVISOR_SUPERVISOR_CLIENT_HPP

#include "device_control.hpp"
#include "screen_control.hpp"

#include "player_runtime/supervisor.hpp"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocalSocket>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>

namespace garlic::core
{
    class PlayerRuntimeApi;
    struct PlayerRuntimeConfig;
}

namespace player
{
    /// What the supervisor said in its answer to hello.
    struct SupervisorHello
    {
        int           protocol = 0;
        QString       name;
        QString       version;
        QSet<QString> capabilities;
        int           heartbeat_interval = 10; // seconds; 0: no hang detection
        bool          locked             = false;
        // start values: only for a new player, see applyStartValues()
        QString uuid;
        QString content_url;
        QString player_name;
    };

    /**
     * The player side of the supervisor protocol (src/core/docs/supervisor_protocol.md): JSON-RPC 2.0, one message per
     * line, over QLocalSocket (Unix domain socket on Linux, named pipe on Windows). Lives in the UI thread.
     *
     *   SupervisorClient supervisor(endpoint, PLAYER_VERSION);
     *   supervisor.start(3s);                 // waits for hello, the uuid is needed before the core starts
     *   config.supervisor = &supervisor;      // reboot, installSoftware from the core thread
     *   supervisor.setCoreProbe(…);           // the heartbeat proves that the core thread runs
     *
     * Commands of the player go out only if the supervisor offers their capability. A lost connection is tried again
     * every 5 s, each time with hello.
     */
    class SupervisorClient final : public QObject,
                                   public garlic::core::Supervisor,
                                   public ScreenControl,
                                   public DeviceControl
    {
            Q_OBJECT

        public:
            static constexpr int protocol_version = 1;
            static constexpr std::chrono::seconds     request_timeout{10};
            static constexpr std::chrono::seconds     reconnect_interval{5};
            static constexpr qsizetype                max_line = 64 * 1024;

            /// Runs a call in the core thread; done() must be called there. Without a probe every tick sends.
            using CoreProbe = std::function<void(std::function<void()> done)>;

            SupervisorClient(QString endpoint, QString player_version, QObject *parent = nullptr);
            ~SupervisorClient() override;

            /// Connects and waits at most timeout for the answer to hello. false: no answer yet, the client keeps
            /// trying in the background (then the start values come too late for this start).
            bool start(std::chrono::milliseconds timeout);

            bool                   ready() const noexcept { return answered; } // hello answered on this connection
            const SupervisorHello &hello() const noexcept { return supervisor; }
            bool                   offers(const QString &capability) const;
            bool                   locked() const noexcept { return supervisor.locked; }

            void setCoreProbe(CoreProbe probe) { core_probe = std::move(probe); }

            // Supervisor of the core, called in the core thread
            void reboot(const std::string &task_id) override;
            void installSoftware(const std::string &path) override;

            // ScreenControl
            void screenOff() override;
            void screenOn() override;
            void deepStandby(std::chrono::seconds wake_up) override;

            /// DeviceControl: setVolume and setBrightness for the values that are set, if offered. Kept and sent
            /// again after every hello, so a supervisor that comes later or restarts gets them too.
            void deviceSettings(const garlic::core::DeviceSettings &settings) override;

            /// Before a wanted end ("user", "quit", "restart"): written before it returns, the player ends next.
            void closing(const QString &reason);

            /// For PlayerRuntimeConfig before start(): the uuid of a new player.
            void seed(garlic::core::PlayerRuntimeConfig &config) const;

            /// After start(), in the core thread: content URL and player name of hello where the player has none.
            static void applyStartValues(garlic::core::PlayerRuntimeApi &api, const SupervisorHello &hello);

        Q_SIGNALS:
            void helloAnswered();                       // also after a reconnect
            void lockedChanged(bool locked);            // hello or setLocked
            void configurationRequested(const QString &path);
            void indexRequested(const QString &uri);
            void quitRequested();                       // answered; the player sends closing("quit") and ends

        private:
            struct Pending
            {
                QString                               method;
                std::chrono::steady_clock::time_point deadline;
            };

            QString         endpoint;
            QString         player_version;
            QLocalSocket    socket;
            QByteArray      buffer;
            QTimer          reconnect;
            QTimer          heartbeat;
            QTimer          timeouts;
            QElapsedTimer   uptime;
            SupervisorHello supervisor;
            bool            answered  = false;
            bool            refused   = false; // -32002: this supervisor does not speak our protocol
            qint64          next_id   = 1;
            std::map<qint64, Pending> pending;

            garlic::core::DeviceSettings      device;
            CoreProbe                         core_probe;
            std::shared_ptr<std::atomic_bool> core_alive = std::make_shared<std::atomic_bool>(true);

            void connectToSupervisor();
            void connected();
            void disconnected();
            void readLines();
            void handle(const QByteArray &line);
            void handleRequest(const QJsonObject &message);
            void handleNotification(const QJsonObject &message);
            void handleResponse(const QJsonObject &message);
            void helloResult(const QJsonObject &result);
            void tick();
            void dropExpired();

            void send(const QJsonObject &message);
            void request(const QString &method, const QJsonObject &params = {});
            void notify(const QString &method, const QJsonObject &params = {});
            void answer(const QJsonValue &id, const QJsonObject &result = {});
            void fail(const QJsonValue &id, int code, const QString &message, const QString &reason = {});
            void command(const QString &capability, const QString &method, const QJsonObject &params = {});
            void sendDeviceSettings();
    };
}

#endif // PLAYER_UI_SUPERVISOR_SUPERVISOR_CLIENT_HPP
