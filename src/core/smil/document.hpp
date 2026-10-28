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
#include <optional>
#include <string>
#include <variant>
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

    // ------------------------------------------------------------------ timing

    /// dur attribute.
    struct Duration
    {
        enum class Type
        {
            None,       // no dur attribute
            Clock,      // dur="10s"
            Indefinite, // dur="indefinite" or dur=""
            Media       // dur="media": as long as the medium plays
        };
        Type                      type = Type::None;
        std::chrono::milliseconds value{0}; // only for Clock
    };

    /// begin="5s", begin="-2s", begin="00:01:30"
    struct Offset
    {
        std::chrono::milliseconds value{0};
    };

    /// begin="indefinite": starts only by a trigger
    struct Indefinite
    {
    };

    /// One value of a begin or end list. Wallclock, syncbase, event, accesskey and notify follow with the triggers.
    using TimeCondition = std::variant<Offset, Indefinite>;

    enum class Restart
    {
        Always,
        WhenNotActive,
        Never
    };

    struct Timing
    {
        std::vector<TimeCondition> begin; // set by the parser: 0s if missing, indefinite in excl
        std::vector<TimeCondition> end;   // empty = no end
        Duration                   dur;
        int                        repeat_count      = 1;
        bool                       repeat_indefinite = false;
        Restart                    restart           = Restart::Always;
    };

    // ------------------------------------------------------------------ media

    enum class MediaKind
    {
        Image,    // <img>, <ref type="image/…">
        Video,    // <video>, <ref type="video/…">
        Audio,    // <audio>, <ref type="audio/…">
        Web,      // <text>, <ref type="text/…">
        Widget,   // <ref type="application/widget">
        Command,  // <ref src="adapi:…">
        Brush,    // <brush color="…">
        Prefetch  // <prefetch>: only download, not played
    };

    enum class CacheControl
    {
        UseCache,
        OnlyIfCached // <param name="cacheControl" value="onlyIfCached">
    };

    struct Media
    {
        MediaKind   kind = MediaKind::Image;
        std::string id;    // like regions: xml:id, id or generated
        std::string title; // debug display only
        std::string expr;  // condition, evaluated with the triggers
        Timing      timing;

        std::string src;         // placeholder "{name}" already replaced
        std::string region;      // region attribute, name or id of a region
        std::string fit;         // empty = default of the region
        std::string media_align; // empty = default of the region
        std::string sound_level; // audio, video
        std::string color;       // brush

        // from <param name="…" value="…">
        std::string  log_content_id; // logContentId: contentId in the play log
        std::string  filename;
        CacheControl cache_control = CacheControl::UseCache;
        std::string  query; // widget: the other params as "?a=1&b=2", percent-encoded, empty if none
    };

    // ------------------------------------------------------------------ tree

    enum class ContainerKind
    {
        Body,
        Seq,
        Par,
        Excl,
        PriorityClass
    };

    /// par: when the par ends without dur or end.
    enum class EndSync
    {
        Last,   // endsync="last" or missing: when all children have ended
        First,  // endsync="first": when the first child ends
        Element // endsync="<id>": when this child ends
    };

    struct Node;

    struct Container
    {
        ContainerKind     kind = ContainerKind::Body;
        std::string       id;
        std::string       title;
        std::string       expr;
        Timing            timing;
        std::vector<Node> children;

        // seq: <metadata> as first child, pick_number random children per run (at least 1)
        std::optional<int> pick_number;

        // par
        EndSync     endsync = EndSync::Last;
        std::string endsync_id; // only for EndSync::Element
    };

    /// An element of the body: a medium or a container with further elements.
    struct Node : std::variant<Media, Container>
    {
        using variant::variant;
    };

    struct Document
    {
        Head      head;
        Container body;
    };
}

#endif // GARLIC_CORE_SMIL_DOCUMENT_HPP
