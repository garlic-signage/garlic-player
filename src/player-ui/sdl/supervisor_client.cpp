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

#include <filesystem>
#include <iostream>

#ifndef _WIN32
    #include <cerrno>
    #include <fcntl.h>
    #include <poll.h>
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <unistd.h>
#endif

namespace player
{
    using nlohmann::json;

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

        std::string platform()
        {
#if defined(__ANDROID__)
            return "android";
#elif defined(_WIN32)
            return "windows";
#elif defined(__APPLE__)
            return "macOS";
#else
            return "linux";
#endif
        }

        bool isUrl(const std::string &uri)
        {
            return uri.starts_with("http://") || uri.starts_with("https://") || uri.starts_with("ftp://");
        }

        bool isFile(const std::string &path)
        {
            std::error_code error;
            return std::filesystem::is_regular_file(path, error);
        }

        long long processId()
        {
#ifdef _WIN32
            return 0;
#else
            return static_cast<long long>(::getpid());
#endif
        }

        void warn(const std::string &text)
        {
            std::cerr << "Supervisor: " << text << "\n";
        }
    }

    SupervisorClient::SupervisorClient(std::string ep, std::string version)
        : endpoint(std::move(ep)), player_version(std::move(version))
    {
    }

    SupervisorClient::~SupervisorClient()
    {
#ifndef _WIN32
        if (socket_fd >= 0)
            ::close(socket_fd);
#endif
    }

    bool SupervisorClient::start(std::chrono::milliseconds timeout)
    {
        auto until = Clock::now() + timeout;
        connectToSupervisor();
#ifndef _WIN32
        while (!answered && socket_fd >= 0 && Clock::now() < until)
        {
            pollfd ready{socket_fd, POLLIN, 0};
            auto   left = std::chrono::duration_cast<std::chrono::milliseconds>(until - Clock::now()).count();
            if (::poll(&ready, 1, static_cast<int>(std::max<long long>(left, 0))) > 0)
                readLines();
        }
#endif
        if (!answered)
            warn("no answer from " + endpoint + " - the player starts without");
        return answered;
    }

    bool SupervisorClient::offers(const std::string &capability) const
    {
        return answered && supervisor.capabilities.contains(capability);
    }

    void SupervisorClient::poll()
    {
        std::vector<std::function<void()>> calls;
        {
            std::lock_guard lock(queue_mutex);
            calls.swap(queued);
        }
        for (auto &call : calls)
            call();

        auto now = Clock::now();
        if (socket_fd < 0)
        {
            if (!refused && now >= next_connect)
                connectToSupervisor();
            return;
        }
        readLines();
        if (answered && supervisor.heartbeat_interval > 0 && now >= next_heartbeat)
        {
            next_heartbeat = now + std::chrono::seconds(supervisor.heartbeat_interval);
            tick();
        }
        dropExpired();
    }

    // ------------------------------------------------------------------ connection

    void SupervisorClient::connectToSupervisor()
    {
#ifdef _WIN32
        refused = true; // named pipes are not done yet
#else
        if (refused || socket_fd >= 0)
            return;
        next_connect = Clock::now() + reconnect_interval;
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        if (endpoint.size() >= sizeof(address.sun_path))
        {
            warn("endpoint too long: " + endpoint);
            refused = true;
            return;
        }
        endpoint.copy(address.sun_path, endpoint.size());
        int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0)
            return;
        if (::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
        {
            ::close(fd);
            return; // tried again after reconnect_interval
        }
        ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        socket_fd = fd;
        connected();
#endif
    }

    void SupervisorClient::connected()
    {
        buffer.clear();
        json params{
            {"protocol", protocol_version},
            {"client", "player"},
            {"player",
             {{"name", "garlic-player"}, {"version", player_version}, {"platform", platform()}, {"pid", processId()}}},
            {"capabilities", json::array({"applyConfiguration", "setIndex", "quit", "setLocked"})},
        };
        request("hello", params);
    }

    void SupervisorClient::disconnected()
    {
#ifndef _WIN32
        if (socket_fd >= 0)
            ::close(socket_fd);
#endif
        socket_fd      = -1;
        bool was_ready = answered;
        answered       = false;
        pending.clear();
        if (was_ready)
            warn("connection lost, trying again every " + std::to_string(reconnect_interval.count()) + " s");
        next_connect = Clock::now() + reconnect_interval;
    }

    void SupervisorClient::readLines()
    {
#ifndef _WIN32
        char chunk[4096];
        while (socket_fd >= 0)
        {
            ssize_t got = ::recv(socket_fd, chunk, sizeof(chunk), 0);
            if (got > 0)
            {
                buffer.append(chunk, static_cast<std::size_t>(got));
                continue;
            }
            if (got == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
            {
                disconnected();
                return;
            }
            if (errno != EINTR)
                break;
        }
#endif
        std::size_t end;
        while ((end = buffer.find('\n')) != std::string::npos)
        {
            std::string line = buffer.substr(0, end);
            buffer.erase(0, end + 1);
            if (line.ends_with('\r'))
                line.pop_back();
            if (line.find_first_not_of(" \t") != std::string::npos)
                handle(line);
            if (socket_fd < 0)
                return;
        }
        if (buffer.size() > max_line)
        {
            warn("a line longer than " + std::to_string(max_line) + " bytes, connection closed");
            buffer.clear();
            disconnected();
        }
    }

    // ------------------------------------------------------------------ incoming

    void SupervisorClient::handle(const std::string &line)
    {
        json message = json::parse(line, nullptr, false);
        if (message.is_discarded() || !message.is_object())
        {
            fail(nullptr, parse_error, "parse error");
            return;
        }
        json id = message.contains("id") ? message["id"] : json();
        if (message.value("jsonrpc", json()) != "2.0")
        {
            fail(id, invalid_request, "invalid request");
            return;
        }
        if (message.contains("method"))
        {
            if (!message["method"].is_string())
                fail(id, invalid_request, "invalid request");
            else if (message.contains("id"))
                handleRequest(message);
            else
                handleNotification(message);
            return;
        }
        if (message.contains("id") && (message.contains("result") || message.contains("error")))
            handleResponse(message);
        else
            fail(id, invalid_request, "invalid request");
    }

    void SupervisorClient::handleRequest(const json &message)
    {
        const json &id     = message["id"];
        std::string method = message["method"].get<std::string>();
        json        params = message.value("params", json::object());
        if (!params.is_object())
            params = json::object();
        if (!answered)
        {
            fail(id, not_ready, "not ready");
            return;
        }
        auto text = [&params](const char *key)
        { return params.contains(key) && params[key].is_string() ? params[key].get<std::string>() : std::string(); };

        if (method == "applyConfiguration")
        {
            std::string path = text("path");
            if (path.empty())
                fail(id, invalid_params, "invalid params", "path missing");
            else if (!isFile(path))
                fail(id, failed, "failed", "file not found");
            else
            {
                answer(id);
                if (handlers.configuration_requested)
                    handlers.configuration_requested(path);
            }
        }
        else if (method == "setIndex")
        {
            std::string uri   = text("uri");
            auto        valid = garlic::core::configuration::Configuration::validateIndexUri(uri);
            if (uri.empty() || !valid)
                fail(id, invalid_params, "invalid params", uri.empty() ? "uri missing" : valid.error());
            else if (!isUrl(*valid) && !isFile(*valid))
                fail(id, failed, "failed", "file not found");
            else
            {
                answer(id);
                if (handlers.index_requested)
                    handlers.index_requested(*valid);
            }
        }
        else if (method == "quit")
        {
            answer(id);
            if (handlers.quit_requested)
                handlers.quit_requested();
        }
        else
            fail(id, method_not_found, "method not found");
    }

    void SupervisorClient::handleNotification(const json &message)
    {
        if (message["method"] != "setLocked" || !answered)
            return; // unknown notifications are ignored
        const json params = message.value("params", json::object());
        if (!params.is_object() || !params.contains("locked") || !params["locked"].is_boolean())
            return;
        bool locked = params["locked"].get<bool>();
        if (locked == supervisor.locked)
            return;
        supervisor.locked = locked;
        if (handlers.locked_changed)
            handlers.locked_changed(locked);
    }

    void SupervisorClient::handleResponse(const json &message)
    {
        const json &id = message["id"];
        if (!id.is_number_integer())
            return;
        auto found = pending.find(id.get<std::int64_t>());
        if (found == pending.end())
            return; // late or unknown: dropped
        std::string method = found->second.method;
        pending.erase(found);

        if (message.contains("error"))
        {
            const json &error  = message["error"];
            int         code   = error.is_object() ? error.value("code", 0) : 0;
            std::string reason;
            if (error.is_object() && error.contains("data") && error["data"].is_object())
                reason = error["data"].value("reason", "");
            warn(method + " failed: " + std::to_string(code) + " " +
                 (error.is_object() ? error.value("message", "") : std::string()) + " " + reason);
            if (method == "hello" && code == not_supported)
            {
                refused = true; // trying again would not help
                disconnected();
            }
            return;
        }
        if (method == "hello")
            helloResult(message["result"].is_object() ? message["result"] : json::object());
    }

    void SupervisorClient::helloResult(const json &result)
    {
        bool was_locked = supervisor.locked;
        auto object     = [&result](const char *key)
        { return result.contains(key) && result[key].is_object() ? result[key] : json::object(); };
        auto text = [](const json &from, const char *key)
        { return from.contains(key) && from[key].is_string() ? from[key].get<std::string>() : std::string(); };

        SupervisorHello next;
        next.protocol = result.contains("protocol") && result["protocol"].is_number_integer() ? result["protocol"].get<int>()
                                                                                                : protocol_version;
        json who      = object("supervisor");
        next.name     = text(who, "name");
        next.version  = text(who, "version");
        if (result.contains("capabilities") && result["capabilities"].is_array())
            for (const json &capability : result["capabilities"])
                if (capability.is_string())
                    next.capabilities.insert(capability.get<std::string>());
        next.heartbeat_interval =
            std::max(0, result.contains("heartbeat_interval") && result["heartbeat_interval"].is_number_integer()
                            ? result["heartbeat_interval"].get<int>()
                            : 10);
        next.locked = result.contains("locked") && result["locked"].is_boolean() && result["locked"].get<bool>();
        json device_json = object("device");
        next.uuid        = text(device_json, "uuid");
        next.content_url = text(device_json, "content_url");
        next.player_name = text(device_json, "player_name");
        supervisor       = next;
        answered         = true;
        std::string capabilities;
        for (const std::string &capability : supervisor.capabilities)
            capabilities += (capabilities.empty() ? "" : ",") + capability;
        std::cerr << "Supervisor: " << supervisor.name << " " << supervisor.version << " capabilities: " << capabilities
                  << "\n";

        core_alive->store(true);
        if (supervisor.heartbeat_interval > 0)
        {
            next_heartbeat = Clock::now() + std::chrono::seconds(supervisor.heartbeat_interval);
            tick(); // the first one at once
        }
        sendDeviceSettings();
        if (supervisor.locked != was_locked && handlers.locked_changed)
            handlers.locked_changed(supervisor.locked);
    }

    // ------------------------------------------------------------------ heartbeat

    /// Each tick asks the core thread for a sign of life; the next tick sends only if it came (both threads proven).
    void SupervisorClient::tick()
    {
        if (!answered)
            return;
        if (core_alive->exchange(false) || !core_probe)
        {
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - started).count();
            notify("heartbeat", {{"uptime", uptime}});
        }
        if (core_probe)
            core_probe([alive = core_alive] { alive->store(true); });
        else
            core_alive->store(true);
    }

    void SupervisorClient::dropExpired()
    {
        auto now = Clock::now();
        for (auto it = pending.begin(); it != pending.end();)
        {
            if (it->second.deadline > now)
            {
                ++it;
                continue;
            }
            warn(it->second.method + " without answer after " + std::to_string(request_timeout.count()) + " s");
            it = pending.erase(it);
        }
    }

    // ------------------------------------------------------------------ outgoing

    void SupervisorClient::reboot(const std::string &task_id)
    {
        std::lock_guard lock(queue_mutex);
        queued.emplace_back([this, task_id] { command("reboot", "reboot", {{"task_id", task_id}}); });
    }

    void SupervisorClient::installSoftware(const std::string &path)
    {
        std::lock_guard lock(queue_mutex);
        queued.emplace_back([this, path] { command("install", "installSoftware", {{"path", path}}); });
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
        command("deep_standby", "deepStandby", {{"seconds_to_wakeup", static_cast<long long>(wake_up.count())}});
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

    void SupervisorClient::closing(const std::string &reason)
    {
        if (!answered)
            return;
        notify("closing", {{"reason", reason}});
    }

    void SupervisorClient::command(const std::string &capability, const std::string &method, const json &params)
    {
        if (!offers(capability))
        {
            warn(method + " not sent, " +
                 (answered ? "the supervisor does not offer " + capability : std::string("no connection")));
            return;
        }
        request(method, params);
    }

    void SupervisorClient::request(const std::string &method, const json &params)
    {
        std::int64_t id = next_id++;
        pending[id]     = {method, Clock::now() + request_timeout};
        json message{{"jsonrpc", "2.0"}, {"id", id}, {"method", method}};
        if (params.is_object() && !params.empty())
            message["params"] = params;
        send(message);
    }

    void SupervisorClient::notify(const std::string &method, const json &params)
    {
        json message{{"jsonrpc", "2.0"}, {"method", method}};
        if (params.is_object() && !params.empty())
            message["params"] = params;
        send(message);
    }

    void SupervisorClient::answer(const json &id, const json &result)
    {
        send({{"jsonrpc", "2.0"}, {"id", id}, {"result", result}});
    }

    void SupervisorClient::fail(const json &id, int code, const std::string &message, const std::string &reason)
    {
        json error{{"code", code}, {"message", message}};
        if (!reason.empty())
            error["data"] = {{"reason", reason}};
        send({{"jsonrpc", "2.0"}, {"id", id}, {"error", error}});
    }

    /// Lines are short: written at once, waiting briefly if the socket is full (like QLocalSocket buffering).
    void SupervisorClient::send(const json &message)
    {
#ifndef _WIN32
        if (socket_fd < 0)
            return;
        std::string line = message.dump() + '\n';
        std::size_t done = 0;
        while (done < line.size())
        {
            ssize_t written = ::send(socket_fd, line.data() + done, line.size() - done, MSG_NOSIGNAL);
            if (written > 0)
            {
                done += static_cast<std::size_t>(written);
                continue;
            }
            if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
            {
                pollfd ready{socket_fd, POLLOUT, 0};
                if (::poll(&ready, 1, 1000) > 0)
                    continue;
            }
            disconnected();
            return;
        }
#else
        (void)message;
#endif
    }

    // ------------------------------------------------------------------ start values

    void SupervisorClient::seed(garlic::core::PlayerRuntimeConfig &config) const
    {
        if (answered && !supervisor.uuid.empty())
            config.uuid = supervisor.uuid; // only for a new player: a stored uuid stays
    }

    /// The player keeps its own: content URL only without one, the name only while it is the one made from the uuid.
    void SupervisorClient::applyStartValues(garlic::core::PlayerRuntimeApi &api, const SupervisorHello &hello)
    {
        garlic::core::PlayerSettings next    = api.playerSettings();
        std::string                  uuid    = api.configuration().uuid();
        bool                         changed = false;
        if (next.index_uri.empty() && !hello.content_url.empty())
        {
            next.index_uri = hello.content_url;
            changed        = true;
        }
        if (!hello.player_name.empty() && next.player_name == uuid.substr(uuid.rfind('-') + 1))
        {
            next.player_name = hello.player_name;
            changed          = true;
        }
        if (!changed)
            return;
        if (auto stored = api.changePlayerSettings(next); !stored)
            warn("start values not taken: " + stored.error());
    }
}
