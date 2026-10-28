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
#ifndef GARLIC_CORE_TESTS_TEST_HTTP_SERVER_HPP
#define GARLIC_CORE_TESTS_TEST_HTTP_SERVER_HPP

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace garlic::core::downloader::tests
{
    /**
     * Minimal HTTP server on 127.0.0.1 for the CurlTransport tests (POSIX only). Answers every request with the
     * response registered for "METHOD /path", one request per connection. Unknown paths get 404.
     */
    class TestHttpServer
    {
        public:
            TestHttpServer()
            {
                listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
                int yes   = 1;
                ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

                sockaddr_in address{};
                address.sin_family      = AF_INET;
                address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                address.sin_port        = 0; // any free port
                ::bind(listen_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address));
                ::listen(listen_fd, 16);

                socklen_t length = sizeof(address);
                ::getsockname(listen_fd, reinterpret_cast<sockaddr *>(&address), &length);
                port = ntohs(address.sin_port);

                worker = std::thread([this] { serve(); });
            }

            ~TestHttpServer()
            {
                stopping = true;
                connectToSelf(); // wakes up accept()
                worker.join();
                ::close(listen_fd);
            }

            /// head: status line and headers without the empty line, e.g. "HTTP/1.1 200 OK\r\nETag: x"
            void route(const std::string &method, const std::string &path, std::string head, std::string body = {})
            {
                std::lock_guard lock(mutex);
                routes[method + " " + path] = {std::move(head), std::move(body)};
            }

            std::string url(const std::string &path) const
            {
                return "http://127.0.0.1:" + std::to_string(port) + path;
            }

            /// Received request heads (request line and headers), in arrival order.
            std::vector<std::string> requests() const
            {
                std::lock_guard lock(mutex);
                return received;
            }

            /// Received request bodies, in arrival order (empty for GET and HEAD).
            std::vector<std::string> bodies() const
            {
                std::lock_guard lock(mutex);
                return received_bodies;
            }

        private:
            struct Response
            {
                std::string head;
                std::string body;
            };

            void serve()
            {
                while (true)
                {
                    int client = ::accept(listen_fd, nullptr, nullptr);
                    if (stopping)
                    {
                        if (client >= 0)
                            ::close(client);
                        return;
                    }
                    if (client < 0)
                        continue;

                    handle(client);
                    ::close(client);
                }
            }

            void handle(int client)
            {
                std::string request;
                char        buffer[4096];
                while (request.find("\r\n\r\n") == std::string::npos)
                {
                    ssize_t n = ::recv(client, buffer, sizeof(buffer), 0);
                    if (n <= 0)
                        return;
                    request.append(buffer, static_cast<std::size_t>(n));
                }

                // a body (PUT): read as many bytes as Content-Length says
                std::size_t head_end = request.find("\r\n\r\n") + 4;
                std::size_t length   = 0;
                if (auto at = request.find("Content-Length: "); at != std::string::npos && at < head_end)
                    length = std::stoul(request.substr(at + 16));
                while (request.size() < head_end + length)
                {
                    ssize_t n = ::recv(client, buffer, sizeof(buffer), 0);
                    if (n <= 0)
                        return;
                    request.append(buffer, static_cast<std::size_t>(n));
                }

                std::string line   = request.substr(0, request.find("\r\n"));
                std::string method = line.substr(0, line.find(' '));
                std::string path   = line.substr(line.find(' ') + 1);
                path               = path.substr(0, path.find(' '));

                Response response{"HTTP/1.1 404 Not Found", "not found"};
                {
                    std::lock_guard lock(mutex);
                    received.push_back(request.substr(0, request.find("\r\n\r\n")));
                    received_bodies.push_back(request.substr(head_end));
                    if (auto it = routes.find(method + " " + path); it != routes.end())
                        response = it->second;
                }

                std::string answer = response.head + "\r\nContent-Length: " + std::to_string(response.body.size()) +
                                     "\r\nConnection: close\r\n\r\n";
                if (method != "HEAD")
                    answer += response.body;

                std::size_t sent = 0;
                while (sent < answer.size())
                {
                    ssize_t n = ::send(client, answer.data() + sent, answer.size() - sent, MSG_NOSIGNAL);
                    if (n <= 0)
                        return;
                    sent += static_cast<std::size_t>(n);
                }
            }

            void connectToSelf() const
            {
                int         fd = ::socket(AF_INET, SOCK_STREAM, 0);
                sockaddr_in address{};
                address.sin_family      = AF_INET;
                address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                address.sin_port        = htons(port);
                ::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address));
                ::close(fd);
            }

            int                             listen_fd = -1;
            std::uint16_t                   port      = 0;
            std::atomic<bool>               stopping  = false;
            std::thread                     worker;
            mutable std::mutex              mutex;
            std::map<std::string, Response> routes;
            std::vector<std::string>        received;
            std::vector<std::string> received_bodies;
    };
}

#endif // GARLIC_CORE_TESTS_TEST_HTTP_SERVER_HPP
