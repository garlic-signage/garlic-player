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
#ifndef GARLIC_CORE_TESTS_RAW_HTTP_HPP
#define GARLIC_CORE_TESTS_RAW_HTTP_HPP

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cstdint>
#include <string>

namespace garlic::core::tests
{
    /// Sends a raw HTTP request to 127.0.0.1 and returns the raw response (blocking: call it from its own thread).
    /// POSIX only.
    inline std::string sendRaw(std::uint16_t port, const std::string &request)
    {
        int     fd = ::socket(AF_INET, SOCK_STREAM, 0);
        timeval limit{10, 0}; // a server that never answers fails the test instead of hanging it
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof(limit));
        sockaddr_in address{};
        address.sin_family      = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port        = htons(port);
        if (::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
        {
            ::close(fd);
            return "connect failed";
        }

        std::size_t sent = 0;
        while (sent < request.size())
        {
            ssize_t n = ::send(fd, request.data() + sent, request.size() - sent, MSG_NOSIGNAL);
            if (n <= 0)
                break;
            sent += static_cast<std::size_t>(n);
        }

        std::string response;
        char        buffer[4096];
        ssize_t     n;
        while ((n = ::recv(fd, buffer, sizeof(buffer), 0)) > 0)
            response.append(buffer, static_cast<std::size_t>(n));
        ::close(fd);
        return response;
    }

    inline int statusOf(const std::string &response)
    {
        auto space = response.find(' ');
        return space == std::string::npos ? 0 : std::stoi(response.substr(space + 1, 3));
    }

    inline std::string bodyOf(const std::string &response)
    {
        auto end = response.find("\r\n\r\n");
        return end == std::string::npos ? "" : response.substr(end + 4);
    }
}

#endif // GARLIC_CORE_TESTS_RAW_HTTP_HPP
