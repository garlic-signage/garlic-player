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
#ifndef GARLIC_CORE_SMIL_DOCUMENT_HPP
#define GARLIC_CORE_SMIL_DOCUMENT_HPP

#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace garlic::core::smil
{
    /// <root-layout>: size of the screen the regions are based on.
    struct RootLayout
    {
        int         width             = 1366;
        int         height            = 768;
        std::string background_color  = "black";
        std::string background_image  = "none";
        std::string background_repeat = "repeat";
    };

    /// <region>: area of the screen in which media are played.
    struct Region
    {
        std::string id = "screen"; // id, xml:id or generated
        std::string region_name;

        // 0..1 relative to the root layout, so the frontend can scale them to any screen size
        double top    = 0;
        double left   = 0;
        double width  = 1;
        double height = 1;
        int    z_index = 0;

        std::string fit               = "hidden";
        std::string media_align       = "topLeft";
        std::string sound_level       = "100%";
        std::string background_color  = "transparent";
        std::string background_image  = "none";
        std::string background_repeat = "repeat";
    };

    /// <subscription> in <metadata>: where reports are sent to or tasks come from.
    struct Subscription
    {
        std::string type;   // SystemReport, InventoryReport, PlaylogCollection, EventlogCollection, TaskSchedule
        std::string action; // URL
        std::string method; // get, put
        int         refresh_interval = 0;
        int         retry_interval   = 0;
    };

    struct Head
    {
        std::string          title = "No Title";
        std::chrono::seconds refresh{0}; // <meta http-equiv="Refresh">, 0 = never look for a new index
        RootLayout           root_layout;
        std::vector<Region>  regions{Region{}}; // sorted by z-index ascending, at least "screen"
        std::vector<Subscription> subscriptions;
        std::map<std::string, std::string, std::less<>> placeholders; // <state><data>: "{name}" → path
    };

    struct Document
    {
        Head head;
    };
}

#endif // GARLIC_CORE_SMIL_DOCUMENT_HPP
