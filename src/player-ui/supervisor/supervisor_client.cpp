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
#include "supervisor_client.hpp"

#include "configuration/configuration.hpp"
#include "player_runtime/player_runtime_api.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSysInfo>

namespace player
{
    namespace
    {
        // the codes of JSON-RPC 2.0 and of the range for implementations (supervisor_protocol.md, Errors)
        constexpr int parse_error      = -32700;
        constexpr int invalid_request  = -32600;
        constexpr int method_not_found = -32601;
        constexpr int invalid_params   = -32602;
        constexpr int failed           = -32000;
        constexpr int not_ready        = -32001;
        constexpr int not_supported    = -32002;

        QString platform()
        {
#if defined(Q_OS_ANDROID)
            return "android";
#elif defined(Q_OS_WIN)
            return "windows";
#elif defined(Q_OS_MACOS)
            return "macOS";
#else
            return "linux";
#endif
        }

        bool isUrl(const QString &uri)
        {
            return uri.startsWith("http://") || uri.startsWith("https://") || uri.startsWith("ftp://");
        }
    }

    SupervisorClient::SupervisorClient(QString ep, QString version, QObject *parent)
        : QObject(parent), endpoint(std::move(ep)), player_version(std::move(version))
    {
        uptime.start();
        reconnect.setSingleShot(true);
        reconnect.setInterval(reconnect_interval);
        connect(&reconnect, &QTimer::timeout, this, &SupervisorClient::connectToSupervisor);
        connect(&heartbeat, &QTimer::timeout, this, &SupervisorClient::tick);
        timeouts.setInterval(std::chrono::seconds(1));
        connect(&timeouts, &QTimer::timeout, this, &SupervisorClient::dropExpired);
        timeouts.start();

        connect(&socket, &QLocalSocket::connected, this, &SupervisorClient::connected);
        connect(&socket, &QLocalSocket::disconnected, this, &SupervisorClient::disconnected);
        connect(&socket, &QLocalSocket::errorOccurred, this,
                [this](QLocalSocket::LocalSocketError)
                {
                    if (socket.state() == QLocalSocket::UnconnectedState)
                        disconnected();
                });
        connect(&socket, &QLocalSocket::readyRead, this, &SupervisorClient::readLines);
    }

    SupervisorClient::~SupervisorClient()
    {
        socket.disconnect(this); // no reconnect while the client goes
        socket.abort();
    }

    bool SupervisorClient::start(std::chrono::milliseconds timeout)
    {
        connectToSupervisor();
        if (answered)
            return true;
        QEventLoop wait;
        QTimer     limit;
        limit.setSingleShot(true);
        connect(&limit, &QTimer::timeout, &wait, &QEventLoop::quit);
        connect(this, &SupervisorClient::helloAnswered, &wait, &QEventLoop::quit);
        limit.start(timeout);
        wait.exec();
        if (!answered)
            qWarning().noquote() << "Supervisor: no answer from" << endpoint << "- the player starts without";
        return answered;
    }

    bool SupervisorClient::offers(const QString &capability) const
    {
        return answered && supervisor.capabilities.contains(capability);
    }

    // ------------------------------------------------------------------ connection

    void SupervisorClient::connectToSupervisor()
    {
        if (refused || socket.state() != QLocalSocket::UnconnectedState)
            return;
        socket.connectToServer(endpoint);
    }

    void SupervisorClient::connected()
    {
        buffer.clear();
        QJsonObject params{
            {"protocol", protocol_version},
            {"client", "player"},
            {"player", QJsonObject{{"name", "garlic-player"},
                                   {"version", player_version},
                                   {"platform", platform()},
                                   {"pid", QCoreApplication::applicationPid()}}},
            {"capabilities", QJsonArray{"applyConfiguration", "setIndex", "quit", "setLocked"}},
        };
        request("hello", params);
    }

    void SupervisorClient::disconnected()
    {
        bool was_ready = answered;
        answered       = false;
        heartbeat.stop();
        pending.clear();
        if (was_ready)
            qWarning().noquote() << "Supervisor: connection lost, trying again every"
                                 << reconnect_interval.count() << "s";
        if (!refused && !reconnect.isActive())
            reconnect.start();
    }

    void SupervisorClient::readLines()
    {
        buffer += socket.readAll();
        qsizetype end;
        while ((end = buffer.indexOf('\n')) >= 0)
        {
            QByteArray line = buffer.left(end);
            buffer.remove(0, end + 1);
            if (line.endsWith('\r'))
                line.chop(1);
            if (!line.trimmed().isEmpty())
                handle(line);
        }
        if (buffer.size() > max_line)
        {
            qWarning() << "Supervisor: a line longer than" << max_line << "bytes, connection closed";
            buffer.clear();
            socket.abort();
        }
    }

    // ------------------------------------------------------------------ incoming

    void SupervisorClient::handle(const QByteArray &line)
    {
        QJsonParseError error;
        QJsonDocument   document = QJsonDocument::fromJson(line, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
        {
            fail(QJsonValue::Null, parse_error, "parse error");
            return;
        }
        QJsonObject message = document.object();
        if (message.value("jsonrpc").toString() != "2.0")
        {
            fail(message.value("id"), invalid_request, "invalid request");
            return;
        }
        if (message.contains("method"))
        {
            if (!message.value("method").isString())
                fail(message.value("id"), invalid_request, "invalid request");
            else if (message.contains("id"))
                handleRequest(message);
            else
                handleNotification(message);
            return;
        }
        if (message.contains("id") && (message.contains("result") || message.contains("error")))
            handleResponse(message);
        else
            fail(message.value("id"), invalid_request, "invalid request");
    }

    void SupervisorClient::handleRequest(const QJsonObject &message)
    {
        QJsonValue  id     = message.value("id");
        QString     method = message.value("method").toString();
        QJsonObject params = message.value("params").toObject();
        if (!answered)
        {
            fail(id, not_ready, "not ready");
            return;
        }

        if (method == "applyConfiguration")
        {
            QString path = params.value("path").toString();
            if (path.isEmpty())
                fail(id, invalid_params, "invalid params", "path missing");
            else if (!QFileInfo(path).isFile())
                fail(id, failed, "failed", "file not found");
            else
            {
                answer(id);
                Q_EMIT configurationRequested(path);
            }
        }
        else if (method == "setIndex")
        {
            QString uri = params.value("uri").toString();
            auto    valid = garlic::core::configuration::Configuration::validateIndexUri(uri.toStdString());
            if (uri.isEmpty() || !valid)
                fail(id, invalid_params, "invalid params", uri.isEmpty() ? "uri missing" : QString::fromStdString(valid.error()));
            else if (QString checked = QString::fromStdString(*valid); !isUrl(checked) && !QFileInfo(checked).isFile())
                fail(id, failed, "failed", "file not found");
            else
            {
                answer(id);
                Q_EMIT indexRequested(checked);
            }
        }
        else if (method == "quit")
        {
            answer(id);
            Q_EMIT quitRequested();
        }
        else
            fail(id, method_not_found, "method not found");
    }

    void SupervisorClient::handleNotification(const QJsonObject &message)
    {
        if (message.value("method").toString() != "setLocked" || !answered)
            return; // unknown notifications are ignored
        QJsonValue locked = message.value("params").toObject().value("locked");
        if (!locked.isBool() || locked.toBool() == supervisor.locked)
            return;
        supervisor.locked = locked.toBool();
        Q_EMIT lockedChanged(supervisor.locked);
    }

    void SupervisorClient::handleResponse(const QJsonObject &message)
    {
        auto found = pending.find(message.value("id").toInteger(-1));
        if (found == pending.end())
            return; // late or unknown: dropped
        QString method = found->second.method;
        pending.erase(found);

        if (message.contains("error"))
        {
            QJsonObject error  = message.value("error").toObject();
            QString     reason = error.value("data").toObject().value("reason").toString();
            qWarning().noquote() << "Supervisor:" << method << "failed:" << error.value("code").toInt()
                                 << error.value("message").toString() << reason;
            if (method == "hello" && error.value("code").toInt() == not_supported)
            {
                refused = true; // trying again would not help
                socket.disconnectFromServer();
            }
            return;
        }
        if (method == "hello")
            helloResult(message.value("result").toObject());
    }

    void SupervisorClient::helloResult(const QJsonObject &result)
    {
        bool was_locked = supervisor.locked;

        SupervisorHello next;
        next.protocol = result.value("protocol").toInt(protocol_version);
        QJsonObject who = result.value("supervisor").toObject();
        next.name       = who.value("name").toString();
        next.version    = who.value("version").toString();
        for (const QJsonValue &capability : result.value("capabilities").toArray())
            if (capability.isString())
                next.capabilities.insert(capability.toString());
        next.heartbeat_interval = std::max(0, result.value("heartbeat_interval").toInt(10));
        next.locked             = result.value("locked").toBool(false);
        QJsonObject device      = result.value("device").toObject();
        next.uuid               = device.value("uuid").toString();
        next.content_url        = device.value("content_url").toString();
        next.player_name        = device.value("player_name").toString();
        supervisor              = next;
        answered                = true;
        qInfo().noquote() << "Supervisor:" << supervisor.name << supervisor.version
                          << "capabilities:" << QStringList(supervisor.capabilities.begin(), supervisor.capabilities.end()).join(",");

        core_alive->store(true);
        if (supervisor.heartbeat_interval > 0)
        {
            heartbeat.start(std::chrono::seconds(supervisor.heartbeat_interval));
            tick(); // the first one at once
        }
        sendDeviceSettings();
        Q_EMIT helloAnswered();
        if (supervisor.locked != was_locked)
            Q_EMIT lockedChanged(supervisor.locked);
    }

    // ------------------------------------------------------------------ heartbeat

    /**
     * The timer runs in the UI thread, so a hanging UI sends nothing. Each tick asks the core thread for a sign of
     * life; the next tick sends the heartbeat only if it came. So the heartbeat proves both threads.
     */
    void SupervisorClient::tick()
    {
        if (!answered)
            return;
        if (core_alive->exchange(false) || !core_probe)
            notify("heartbeat", QJsonObject{{"uptime", uptime.elapsed() / 1000}});
        if (core_probe)
            core_probe([alive = core_alive] { alive->store(true); });
        else
            core_alive->store(true);
    }

    void SupervisorClient::dropExpired()
    {
        auto now = std::chrono::steady_clock::now();
        for (auto it = pending.begin(); it != pending.end();)
        {
            if (it->second.deadline > now)
            {
                ++it;
                continue;
            }
            qWarning().noquote() << "Supervisor:" << it->second.method << "without answer after"
                                 << request_timeout.count() << "s";
            it = pending.erase(it);
        }
    }

    // ------------------------------------------------------------------ outgoing

    void SupervisorClient::reboot(const std::string &task_id)
    {
        QString id = QString::fromStdString(task_id);
        QMetaObject::invokeMethod(this, [this, id] { command("reboot", "reboot", {{"task_id", id}}); },
                                  Qt::QueuedConnection);
    }

    void SupervisorClient::installSoftware(const std::string &path)
    {
        QString file = QString::fromStdString(path);
        QMetaObject::invokeMethod(this, [this, file] { command("install", "installSoftware", {{"path", file}}); },
                                  Qt::QueuedConnection);
    }

    void SupervisorClient::screenOff()
    {
        command("screen", "screenOff");
    }

    void SupervisorClient::screenOn()
    {
        command("screen", "screenOn");
    }

    void SupervisorClient::deepStandby(std::chrono::seconds wake_up)
    {
        command("deep_standby", "deepStandby", {{"seconds_to_wakeup", static_cast<qint64>(wake_up.count())}});
    }

    void SupervisorClient::deviceSettings(const garlic::core::DeviceSettings &settings)
    {
        device = settings;
        if (answered)
            sendDeviceSettings();
    }

    /// Only what is set and offered; quietly, a supervisor without volume or brightness is no error.
    void SupervisorClient::sendDeviceSettings()
    {
        if (device.volume && offers("volume"))
            request("setVolume", {{"level", *device.volume}});
        if (device.brightness && offers("brightness"))
            request("setBrightness", {{"level", *device.brightness}});
    }

    void SupervisorClient::closing(const QString &reason)
    {
        if (!answered)
            return;
        notify("closing", QJsonObject{{"reason", reason}});
        socket.flush();
        socket.waitForBytesWritten(1000);
    }

    void SupervisorClient::command(const QString &capability, const QString &method, const QJsonObject &params)
    {
        if (!offers(capability))
        {
            qWarning().noquote() << "Supervisor:" << method << "not sent,"
                                 << (answered ? "the supervisor does not offer " + capability : QString("no connection"));
            return;
        }
        request(method, params);
    }

    void SupervisorClient::request(const QString &method, const QJsonObject &params)
    {
        qint64 id   = next_id++;
        pending[id] = {method, std::chrono::steady_clock::now() + request_timeout};
        QJsonObject message{{"jsonrpc", "2.0"}, {"id", id}, {"method", method}};
        if (!params.isEmpty())
            message.insert("params", params);
        send(message);
    }

    void SupervisorClient::notify(const QString &method, const QJsonObject &params)
    {
        QJsonObject message{{"jsonrpc", "2.0"}, {"method", method}};
        if (!params.isEmpty())
            message.insert("params", params);
        send(message);
    }

    void SupervisorClient::answer(const QJsonValue &id, const QJsonObject &result)
    {
        send(QJsonObject{{"jsonrpc", "2.0"}, {"id", id}, {"result", result}});
    }

    void SupervisorClient::fail(const QJsonValue &id, int code, const QString &message, const QString &reason)
    {
        QJsonObject error{{"code", code}, {"message", message}};
        if (!reason.isEmpty())
            error.insert("data", QJsonObject{{"reason", reason}});
        send(QJsonObject{{"jsonrpc", "2.0"}, {"id", id.isUndefined() ? QJsonValue(QJsonValue::Null) : id}, {"error", error}});
    }

    void SupervisorClient::send(const QJsonObject &message)
    {
        if (socket.state() != QLocalSocket::ConnectedState)
            return;
        socket.write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
    }

    // ------------------------------------------------------------------ start values

    void SupervisorClient::seed(garlic::core::PlayerRuntimeConfig &config) const
    {
        if (answered && !supervisor.uuid.isEmpty())
            config.uuid = supervisor.uuid.toStdString(); // only for a new player: a stored uuid stays
    }

    /// The player keeps its own: content URL only without one, the name only while it is the one made from the uuid.
    void SupervisorClient::applyStartValues(garlic::core::PlayerRuntimeApi &api, const SupervisorHello &hello)
    {
        garlic::core::PlayerSettings next = api.playerSettings();
        std::string                  uuid = api.configuration().uuid();
        bool                         changed = false;
        if (next.index_uri.empty() && !hello.content_url.isEmpty())
        {
            next.index_uri = hello.content_url.toStdString();
            changed        = true;
        }
        if (!hello.player_name.isEmpty() && next.player_name == uuid.substr(uuid.rfind('-') + 1))
        {
            next.player_name = hello.player_name.toStdString();
            changed          = true;
        }
        if (!changed)
            return;
        if (auto stored = api.changePlayerSettings(next); !stored)
            qWarning().noquote() << "Supervisor: start values not taken:" << QString::fromStdString(stored.error());
    }
}
