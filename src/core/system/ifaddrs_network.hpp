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
#ifndef GARLIC_CORE_SYSTEM_IFADDRS_NETWORK_HPP
#define GARLIC_CORE_SYSTEM_IFADDRS_NETWORK_HPP

#include "reports/system_info.hpp"

#include <ifaddrs.h>

#include <string>
#include <vector>

/// Network interfaces with getifaddrs, shared by Linux/Android and macOS/iOS. Only the link layer differs.
namespace garlic::core::system::platform
{
    /// Fills type and MAC of an interface from its link layer entry (AF_PACKET on Linux, AF_LINK on Apple).
    using LinkLayer = void (*)(const ifaddrs &ifa, reports::NetworkInterface &item);

    /// "00:1A:2B:3C:4D:5E", upper case like Qt
    std::string macText(const unsigned char *bytes, int length);

    /// The interfaces with a MAC and at least one address (garlic-lib: isRealInterface).
    std::vector<reports::NetworkInterface> ifaddrsNetwork(int link_family, LinkLayer link_layer);
}

#endif // GARLIC_CORE_SYSTEM_IFADDRS_NETWORK_HPP
