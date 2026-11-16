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
#ifndef PLAYER_UI_SDL_SUPERVISOR_CLIENT_HPP
#define PLAYER_UI_SDL_SUPERVISOR_CLIENT_HPP

#include "blank_screen.hpp"

#include "player_runtime/supervisor.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

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
        int                   protocol = 0;
        std::string           name;
        std::string           version;
        std::set<std::string> capabilities;
        int                   heartbeat_interval = 10; // seconds; 0: no hang detection
        bool                  locked             = false;
        // start values: only for a new player, see applyStartValues()
        std::string uuid;
        std::string content_url;
        std::string player_name;
    };

    /**
     * The player side of the supervisor protocol (src/core/docs/supervisor_protocol.md) without Qt: JSON-RPC 2.0, one
     * message per line, over a Unix domain socket. The same behavior as the client of the Qt players
     * (src/player-ui/supervisor/supervisor_client.hpp); the main loop calls poll() at least every 100 ms.
     *
     * Windows (named pipe) is not done yet: there start() reports that no supervisor answered.
     */
    class SupervisorClient final : public garlic::core::Supervisor, public ScreenControl, public DeviceControl
    {
        public:
            static constexpr int                  protocol_version = 1;
            static constexpr std::chrono::seconds request_timeout{10};
            static constexpr std::chrono::seconds reconnect_interval{5};
            static constexpr std::size_t          max_line = 64 * 1024;

            /// Runs a call in the core thread; done() must be called there. Without a probe every tick sends.
            using CoreProbe = std::function<void(std::function<void()> done)>;

            /// What the supervisor asks of the player; called from poll() in the UI thread.
            struct Handlers
            {
                    std::function<void(bool locked)>             locked_changed;
                    std::function<void(const std::string &path)> configuration_requested;
                    std::function<void(const std::string &uri)>  index_requested;
                    std::function<void()>                        quit_requested; // answered; send closing("quit")
            };

            SupervisorClient(std::string endpoint, std::string player_version);
            ~SupervisorClient() override;

            /// Connects and waits at most timeout for the answer to hello. false: no answer yet, the client keeps
            /// trying in poll() (then the start values come too late for this start).
            bool start(std::chrono::milliseconds timeout);

            /// Reads, answers, sends heartbeats and what the core thread asked for, connects again. UI thread.
            void poll();

            [[nodiscard]] bool                   ready() const noexcept { return answered; }
            [[nodiscard]] const SupervisorHello &hello() const noexcept { return supervisor; }
            [[nodiscard]] bool                   offers(const std::string &capability) const;
            [[nodiscard]] bool                   locked() const noexcept { return supervisor.locked; }

            void setCoreProbe(CoreProbe probe) { core_probe = std::move(probe); }
            void setHandlers(Handlers next) { handlers = std::move(next); }

            // Supervisor of the core, called in the core thread: queued for poll()
            void reboot(const std::string &task_id) override;
            void installSoftware(const std::string &path) override;

            // ScreenControl
            void screenOff() override;
            void screenOn() override;
            void deepStandby(std::chrono::seconds wake_up) override;

            /// DeviceControl: setVolume and setBrightness for the values that are set, if offered; kept and sent
            /// again after every hello.
            void deviceSettings(const garlic::core::DeviceSettings &settings) override;

            /// Before a wanted end ("user", "quit", "restart"): written before it returns.
            void closing(const std::string &reason);

            /// For PlayerRuntimeConfig before start(): the uuid of a new player.
            void seed(garlic::core::PlayerRuntimeConfig &config) const;

            /// After start(), in the core thread: content URL and player name of hello where the player has none.
            static void applyStartValues(garlic::core::PlayerRuntimeApi &api, const SupervisorHello &hello);

        private:
            using Clock = std::chrono::steady_clock;

            struct Pending
            {
                    std::string       method;
                    Clock::time_point deadline;
            };

            std::string     endpoint;
            std::string     player_version;
            int             socket_fd = -1;
            std::string     buffer;
            SupervisorHello supervisor;
            bool            answered = false;
            bool            refused  = false; // -32002: this supervisor does not speak our protocol
            std::int64_t    next_id  = 1;
            std::map<std::int64_t, Pending> pending;

            Clock::time_point started = Clock::now();
            Clock::time_point next_connect{};
            Clock::time_point next_heartbeat{};

            garlic::core::DeviceSettings      device;
            CoreProbe                         core_probe;
            Handlers                          handlers;
            std::shared_ptr<std::atomic_bool> core_alive = std::make_shared<std::atomic_bool>(true);

            std::mutex                         queue_mutex;
            std::vector<std::function<void()>> queued; // from the core thread

            void connectToSupervisor();
            void connected();
            void disconnected();
            void readLines();
            void handle(const std::string &line);
            void handleRequest(const nlohmann::json &message);
            void handleNotification(const nlohmann::json &message);
            void handleResponse(const nlohmann::json &message);
            void helloResult(const nlohmann::json &result);
            void tick();
            void dropExpired();

            void send(const nlohmann::json &message);
            void request(const std::string &method, const nlohmann::json &params = nullptr);
            void notify(const std::string &method, const nlohmann::json &params = nullptr);
            void answer(const nlohmann::json &id, const nlohmann::json &result = nlohmann::json::object());
            void fail(const nlohmann::json &id, int code, const std::string &message, const std::string &reason = {});
            void command(const std::string &capability, const std::string &method,
                         const nlohmann::json &params = nullptr);
            void sendDeviceSettings();
    };
}

#endif // PLAYER_UI_SDL_SUPERVISOR_CLIENT_HPP
