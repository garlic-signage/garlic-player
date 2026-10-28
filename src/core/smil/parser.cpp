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
#include "smil/parser.hpp"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <algorithm>
#include <charconv>
#include <climits>
#include <cmath>
#include <memory>
#include <optional>
#include <ranges>

namespace garlic::core::smil
{
    namespace
    {
        struct DocDeleter
        {
            void operator()(xmlDoc *doc) const { xmlFreeDoc(doc); }
        };

        struct ContextDeleter
        {
            void operator()(xmlParserCtxt *ctxt) const { xmlFreeParserCtxt(ctxt); }
        };

        const xmlChar *xml(const char *text)
        {
            return reinterpret_cast<const xmlChar *>(text);
        }

        /// Local name without namespace prefix, like QDom without namespace processing.
        std::string_view name(const xmlNode *node)
        {
            return reinterpret_cast<const char *>(node->name);
        }

        std::string takeString(xmlChar *value)
        {
            std::string result(reinterpret_cast<const char *>(value));
            xmlFree(value);
            return result;
        }

        std::optional<std::string> attribute(const xmlNode *node, const char *attribute_name)
        {
            xmlChar *value = xmlGetNoNsProp(node, xml(attribute_name));
            if (value == nullptr)
                return std::nullopt;
            return takeString(value);
        }

        void readAttribute(const xmlNode *node, const char *attribute_name, std::string &target)
        {
            if (auto value = attribute(node, attribute_name))
                target = std::move(*value);
        }

        std::string text(const xmlNode *node)
        {
            xmlChar *value = xmlNodeGetContent(node);
            return value == nullptr ? std::string() : takeString(value);
        }

        std::string_view trim(std::string_view s)
        {
            auto first = s.find_first_not_of(" \t\r\n");
            if (first == std::string_view::npos)
                return {};
            return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
        }

        std::string toUpper(std::string_view s)
        {
            std::string out(s);
            for (char &c : out)
                if (c >= 'a' && c <= 'z')
                    c = static_cast<char>(c - 'a' + 'A');
            return out;
        }

        /// Leading number of a value, so "1920px" is 1920; 0 if there is none (like QString::toInt).
        int toInt(std::string_view s)
        {
            s         = trim(s);
            int value = 0;
            std::from_chars(s.data(), s.data() + s.size(), value);
            return value;
        }

        double toDouble(std::string_view s)
        {
            s            = trim(s);
            double value = 0;
            std::from_chars(s.data(), s.data() + s.size(), value);
            return value;
        }

        /// The whole value as number, 0 otherwise (like QString::toFloat). For time values, so "30d" is not 30 s.
        double toNumber(std::string_view s)
        {
            double value = 0;
            auto [end, error] = std::from_chars(s.data(), s.data() + s.size(), value);
            return error == std::errc() && end == s.data() + s.size() ? value : 0;
        }

        /// "50%" → 0.5, "960px" or "960" → 960 / root. Empty values keep the default.
        std::optional<double> relative(std::string_view value, int root)
        {
            value = trim(value);
            if (value.empty())
                return std::nullopt;
            if (value.ends_with('%'))
                return toDouble(value.substr(0, value.size() - 1)) / 100;
            if (value.ends_with("px"))
                value.remove_suffix(2);
            return root > 0 ? toDouble(value) / root : 0;
        }

        template <typename Visitor> void forEachChild(const xmlNode *parent, Visitor visit)
        {
            for (const xmlNode *child = parent->children; child != nullptr; child = child->next)
                if (child->type == XML_ELEMENT_NODE)
                    visit(child);
        }

        /// All descendants with this name, like QDomElement::elementsByTagName.
        void findDescendants(const xmlNode *parent, std::string_view tag, std::vector<const xmlNode *> &found)
        {
            forEachChild(parent,
                         [&](const xmlNode *child)
                         {
                             if (name(child) == tag)
                                 found.push_back(child);
                             findDescendants(child, tag, found);
                         });
        }

        std::vector<const xmlNode *> descendants(const xmlNode *parent, std::string_view tag)
        {
            std::vector<const xmlNode *> found;
            findDescendants(parent, tag, found);
            return found;
        }

        std::vector<std::string_view> split(std::string_view s, char separator)
        {
            std::vector<std::string_view> parts;
            for (auto part : std::views::split(s, separator))
                parts.emplace_back(part.begin(), part.end());
            return parts;
        }

        // ------------------------------------------------------------------ timing values

        /**
         * SMIL clock value in milliseconds, garlic-lib: ClockValue.
         *
         *   "01:30:00.5"  full clock      h:m:s
         *   "1:30"        partial clock   m:s
         *   "2h" "1.5min" "90s" "500ms"   timecount
         *   "90"          no unit = seconds
         *   "+5s" "-2s"   with sign (offsets of begin/end)
         */
        std::chrono::milliseconds parseClockValue(std::string_view value)
        {
            value         = trim(value);
            bool negative = false;
            if (value.starts_with('+') || value.starts_with('-'))
            {
                negative = value.front() == '-';
                value.remove_prefix(1);
            }

            double ms = 0;
            if (value.contains(':'))
            {
                auto parts = split(value, ':');
                if (parts.size() == 3)
                    ms = toNumber(parts[0]) * 3600000.0 + toNumber(parts[1]) * 60000.0 + toNumber(parts[2]) * 1000;
                else if (parts.size() == 2)
                    ms = toNumber(parts[0]) * 60000.0 + toNumber(parts[1]) * 1000;
            }
            else if (value.ends_with("ms")) // before "s", garlic-lib read "500ms" as 0
                ms = toNumber(value.substr(0, value.size() - 2));
            else if (value.ends_with("min"))
                ms = toNumber(value.substr(0, value.size() - 3)) * 60000;
            else if (value.ends_with('h'))
                ms = toNumber(value.substr(0, value.size() - 1)) * 3600000;
            else if (value.ends_with('s'))
                ms = toNumber(value.substr(0, value.size() - 1)) * 1000;
            else
                ms = toNumber(value) * 1000;

            std::chrono::milliseconds result(std::llround(ms));
            return negative ? -result : result;
        }

        /// dur="10s" | "indefinite" | "" | "media"
        Duration parseDuration(std::string_view value)
        {
            value = trim(value);
            if (value.empty() || value == "indefinite")
                return {Duration::Type::Indefinite, {}};
            if (value == "media")
                return {Duration::Type::Media, {}};
            return {Duration::Type::Clock, std::max(parseClockValue(value), std::chrono::milliseconds(0))};
        }

        /**
         * begin/end: list separated by ";", e.g. "0s;10s", garlic-lib: EnhancedTimer::parse.
         * In a seq only the first value counts. In a list with several values "indefinite" is ignored.
         */
        std::vector<TimeCondition> parseTimeList(std::string_view value, ContainerKind parent)
        {
            std::vector<std::string_view> values;
            for (auto part : split(value, ';'))
                if (auto v = trim(part); !v.empty())
                    values.push_back(v);
            if (parent == ContainerKind::Seq && values.size() > 1)
                values.resize(1);

            std::vector<TimeCondition> conditions;
            for (auto v : values)
            {
                if (v.contains("indefinite"))
                {
                    if (values.size() == 1)
                        conditions.emplace_back(Indefinite{});
                }
                else if (v.front() == '+' || v.front() == '-' || (v.front() >= '0' && v.front() <= '9'))
                    conditions.emplace_back(Offset{parseClockValue(v)});
                // wallclock(…), accesskey(…), notify(…), id.begin, id.event: follow with the triggers
            }
            return conditions;
        }

        /// <… begin end dur repeatCount restart>, garlic-lib: BaseTimings::parseTimingAttributes
        Timing parseTiming(const xmlNode *node, ContainerKind parent)
        {
            Timing timing;
            if (auto begin = attribute(node, "begin"))
                timing.begin = parseTimeList(*begin, parent);
            if (timing.begin.empty()) // missing or not readable
            {
                if (parent == ContainerKind::Excl)
                    timing.begin.emplace_back(Indefinite{});
                else
                    timing.begin.emplace_back(Offset{});
            }

            if (auto end = attribute(node, "end"))
                timing.end = parseTimeList(*end, parent);
            if (auto dur = attribute(node, "dur"))
                timing.dur = parseDuration(*dur);

            if (auto repeat = attribute(node, "repeatCount"))
            {
                if (trim(*repeat) == "indefinite")
                    timing.repeat_indefinite = true;
                else
                    timing.repeat_count = std::max(toInt(*repeat), 1); // "2.5" → 2
            }

            if (auto restart = attribute(node, "restart"))
            {
                if (*restart == "always")
                    timing.restart = Restart::Always;
                else if (*restart == "never")
                    timing.restart = Restart::Never;
                else
                    timing.restart = Restart::WhenNotActive;
            }
            return timing;
        }

        // ------------------------------------------------------------------ media

        /**
         * Which medium an element is, garlic-lib: ElementFactory. Unknown elements are not part of the tree.
         *
         *   <img> <video> <audio> <brush> <prefetch>  as named
         *   <text>                                    Web
         *   <ref type="image/…|video/…|audio/…|text/…|application/widget">
         *   <ref src="adapi:…"> without type          Command
         */
        std::optional<MediaKind> mediaKind(const xmlNode *node)
        {
            std::string_view tag = name(node);
            if (tag == "img")
                return MediaKind::Image;
            if (tag == "video")
                return MediaKind::Video;
            if (tag == "audio")
                return MediaKind::Audio;
            if (tag == "text")
                return MediaKind::Web;
            if (tag == "brush")
                return MediaKind::Brush;
            if (tag == "prefetch")
                return MediaKind::Prefetch;
            if (tag != "ref")
                return std::nullopt;

            if (auto type = attribute(node, "type"))
            {
                if (type->contains("image"))
                    return MediaKind::Image;
                if (type->contains("video"))
                    return MediaKind::Video;
                if (type->contains("audio"))
                    return MediaKind::Audio;
                if (type->contains("text"))
                    return MediaKind::Web;
                if (type->contains("application/widget"))
                    return MediaKind::Widget;
                return std::nullopt;
            }
            if (attribute(node, "src").value_or("").contains("adapi:"))
                return MediaKind::Command;
            return std::nullopt;
        }

        /// Like QUrl::toPercentEncoding: everything except A-Z a-z 0-9 - . _ ~ becomes %XX.
        std::string percentEncode(std::string_view text)
        {
            constexpr char hex[] = "0123456789ABCDEF";
            std::string    out;
            for (char ch : text)
            {
                auto c = static_cast<unsigned char>(ch);
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
                    c == '.' || c == '_' || c == '~')
                    out.push_back(static_cast<char>(c));
                else
                {
                    out.push_back('%');
                    out.push_back(hex[c >> 4]);
                    out.push_back(hex[c & 0x0F]);
                }
            }
            return out;
        }

        class Parser
        {
            public:
                /// <smil><head>…</head><body>…</body></smil>; the head first, the body needs its placeholders
                Document parse(const xmlNode *smil)
                {
                    Document document;
                    forEachChild(smil,
                                 [&](const xmlNode *child)
                                 {
                                     if (name(child) == "head")
                                         parseHead(child, document.head);
                                 });
                    forEachChild(smil,
                                 [&](const xmlNode *child)
                                 {
                                     if (name(child) == "body")
                                         document.body = parseContainer(child, ContainerKind::Body,
                                                                        ContainerKind::Body, document.head);
                                 });
                    return document;
                }

            private:
                int generated_ids = 0;

                /// id, xml:id (SMIL 3.0) or "<tag>_<line>_<number>" if the SMIL has none.
                std::string parseId(const xmlNode *node)
                {
                    xmlChar *xml_id = xmlGetNsProp(node, xml("id"), XML_XML_NAMESPACE);
                    if (xml_id != nullptr)
                        return takeString(xml_id);
                    if (auto id = attribute(node, "id"); id && !id->empty())
                        return *id;
                    return std::string(name(node)) + "_" + std::to_string(xmlGetLineNo(node)) + "_" +
                           std::to_string(++generated_ids);
                }

                void parseHead(const xmlNode *node, Head &head)
                {
                    forEachChild(node,
                                 [&](const xmlNode *child)
                                 {
                                     std::string_view tag = name(child);
                                     if (tag == "meta")
                                         parseMeta(child, head);
                                     else if (tag == "metadata")
                                         parseMetadata(child, head);
                                     else if (tag == "layout")
                                         parseLayout(child, head);
                                     else if (tag == "state")
                                         parseState(child, head);
                                 });
                }

                void parseMeta(const xmlNode *node, Head &head)
                {
                    auto content = attribute(node, "content");
                    if (!content)
                        return;
                    if (attribute(node, "name") == "title")
                        head.title = *content;
                    else if (attribute(node, "http-equiv") == "Refresh")
                        head.refresh = std::chrono::seconds(toInt(*content));
                }

                void parseMetadata(const xmlNode *node, Head &head)
                {
                    for (const xmlNode *element : descendants(node, "subscription"))
                    {
                        Subscription subscription;
                        forEachChild(element,
                                     [&](const xmlNode *child)
                                     {
                                         std::string_view tag   = name(child);
                                         std::string      value = std::string(trim(text(child)));
                                         if (tag == "type")
                                             subscription.type = value;
                                         else if (tag == "action")
                                             subscription.action = value;
                                         else if (tag == "method")
                                             subscription.method = value;
                                         else if (tag == "refreshInterval")
                                             subscription.refresh_interval = toInt(value);
                                         else if (tag == "retryInterval")
                                             subscription.retry_interval = toInt(value);
                                     });
                        head.subscriptions.push_back(std::move(subscription));
                    }
                }

                void parseLayout(const xmlNode *node, Head &head)
                {
                    // root-layout first, the regions are based on its size
                    auto root_layouts = descendants(node, "root-layout");
                    if (root_layouts.size() == 1)
                        parseRootLayout(root_layouts.front(), head.root_layout);

                    auto regions = descendants(node, "region");
                    if (regions.empty())
                        return;

                    head.regions.clear(); // replaces the default region "screen"
                    for (const xmlNode *element : regions)
                        head.regions.push_back(parseRegion(element, head.root_layout));

                    // stable: regions with the same z-index keep the order of the SMIL
                    std::ranges::stable_sort(head.regions, {}, &Region::z_index);
                }

                static void parseRootLayout(const xmlNode *node, RootLayout &root)
                {
                    if (auto width = attribute(node, "width"))
                        root.width = toInt(*width);
                    if (auto height = attribute(node, "height"))
                        root.height = toInt(*height);
                    readAttribute(node, "backgroundColor", root.background_color);
                    readAttribute(node, "backgroundImage", root.background_image);
                    readAttribute(node, "backgroundRepeat", root.background_repeat);
                }

                Region parseRegion(const xmlNode *node, const RootLayout &root)
                {
                    Region region;
                    region.id = parseId(node);
                    readAttribute(node, "regionName", region.region_name);

                    auto readRelative = [&](const char *attribute_name, int base, double &target)
                    {
                        if (auto value = attribute(node, attribute_name))
                            target = relative(*value, base).value_or(target);
                    };
                    readRelative("top", root.height, region.top);
                    readRelative("left", root.width, region.left);
                    readRelative("width", root.width, region.width);
                    readRelative("height", root.height, region.height);

                    if (auto z_index = attribute(node, "z-index"))
                        region.z_index = toInt(*z_index);
                    readAttribute(node, "fit", region.fit);
                    readAttribute(node, "mediaAlign", region.media_align);
                    readAttribute(node, "soundLevel", region.sound_level);
                    readAttribute(node, "backgroundColor", region.background_color);
                    readAttribute(node, "backgroundImage", region.background_image);
                    readAttribute(node, "backgroundRepeat", region.background_repeat);
                    return region;
                }

                /// <state><data><name>path</name></data></state>: placeholders for src="{name}"
                static void parseState(const xmlNode *node, Head &head)
                {
                    forEachChild(node,
                                 [&](const xmlNode *data)
                                 {
                                     if (name(data) != "data")
                                         return;
                                     forEachChild(data,
                                                  [&](const xmlNode *entry)
                                                  {
                                                      std::string path = text(entry);
                                                      if (!path.empty())
                                                          head.placeholders["{" + std::string(name(entry)) + "}"] =
                                                              std::move(path);
                                                  });
                                 });
                }

                /**
                 * <body>, <seq>, <par>: the container and its children, nested containers recursively.
                 * parent is the kind of the surrounding container, it decides the defaults of begin.
                 * garlic-lib: TBody, TSeq, TPar
                 */
                Container parseContainer(const xmlNode *node, ContainerKind kind, ContainerKind parent,
                                         const Head &head)
                {
                    Container container;
                    container.kind   = kind;
                    container.id     = parseId(node);
                    container.timing = parseTiming(node, parent);
                    readAttribute(node, "title", container.title);
                    readAttribute(node, "expr", container.expr);
                    if (kind == ContainerKind::Par)
                        parseEndSync(node, container);

                    bool first = true;
                    forEachChild(node,
                                 [&](const xmlNode *child)
                                 {
                                     std::string_view tag = name(child);
                                     if (tag == "metadata")
                                     {
                                         if (kind == ContainerKind::Seq && first)
                                             container.pick_number = parsePickNumber(child);
                                     }
                                     else if (tag == "seq")
                                         container.children.emplace_back(
                                             parseContainer(child, ContainerKind::Seq, kind, head));
                                     else if (tag == "par")
                                         container.children.emplace_back(
                                             parseContainer(child, ContainerKind::Par, kind, head));
                                     else if (auto media_kind = mediaKind(child))
                                         container.children.emplace_back(
                                             parseMedia(child, *media_kind, kind, head));
                                     // excl: next step
                                     first = false;
                                 });
                    return container;
                }

                /**
                 * <par endsync="last|first|<id>">, garlic-lib: TPar::interruptByEndSync.
                 * Any other value is taken as id; if no child has it, the par behaves like "last".
                 */
                static void parseEndSync(const xmlNode *node, Container &container)
                {
                    auto endsync = attribute(node, "endsync");
                    if (!endsync || *endsync == "last")
                        return;
                    if (*endsync == "first")
                        container.endsync = EndSync::First;
                    else
                    {
                        container.endsync    = EndSync::Element;
                        container.endsync_id = *endsync;
                    }
                }

                /**
                 * <seq>
                 *     <metadata>                      only as first child
                 *         <meta name="adapi:pickingAlgorithm" content="shuffle"/>   always shuffle, not read
                 *         <meta name="adapi:pickingBehavior" content="pickN"/>      always pickN, not read
                 *         <meta name="adapi:pickNumber" content="2"/>
                 *     </metadata>
                 * garlic-lib: TShuffle. Without pickNumber 1, values below 1 become 1.
                 */
                static int parsePickNumber(const xmlNode *metadata)
                {
                    int pick_number = 1;
                    forEachChild(metadata,
                                 [&](const xmlNode *meta)
                                 {
                                     auto content = attribute(meta, "content");
                                     if (name(meta) == "meta" && content &&
                                         attribute(meta, "name") == "adapi:pickNumber")
                                         pick_number = toInt(*content);
                                 });
                    return std::max(pick_number, 1);
                }

                /**
                 * <img src="a.jpg" region="screen" dur="10s" fit="meet">
                 *     <param name="logContentId" value="123"/>
                 * </img>
                 * garlic-lib: BaseMedia and its subclasses TImage, TVideo, TAudio, TWeb, TWidget, TBrush, …
                 */
                Media parseMedia(const xmlNode *node, MediaKind kind, ContainerKind parent, const Head &head)
                {
                    Media media;
                    media.kind   = kind;
                    media.id     = parseId(node);
                    media.timing = parseTiming(node, parent);
                    readAttribute(node, "title", media.title);
                    readAttribute(node, "expr", media.expr);
                    readAttribute(node, "region", media.region);
                    readAttribute(node, "fit", media.fit);
                    readAttribute(node, "mediaAlign", media.media_align);

                    if (kind == MediaKind::Brush)
                        readAttribute(node, "color", media.color);
                    else
                        media.src = resolvePlaceholder(attribute(node, "src").value_or(""), head);
                    if (kind == MediaKind::Audio || kind == MediaKind::Video)
                        readAttribute(node, "soundLevel", media.sound_level);

                    parseParams(node, media);
                    return media;
                }

                /// src="{logo}" → path from <state><data><logo>; unknown placeholders become "" like in garlic-lib
                static std::string resolvePlaceholder(std::string src, const Head &head)
                {
                    if (!src.starts_with('{') || !src.ends_with('}'))
                        return src;
                    auto found = head.placeholders.find(src);
                    return found == head.placeholders.end() ? std::string() : found->second;
                }

                /**
                 * <param name="cacheControl" value="onlyIfCached"/>
                 * <param name="filename"     value="…"/>
                 * <param name="logContentId" value="…"/>
                 * every other param of a widget is appended to its URL: ?name=value&…
                 */
                static void parseParams(const xmlNode *node, Media &media)
                {
                    forEachChild(node,
                                 [&](const xmlNode *param)
                                 {
                                     if (name(param) != "param")
                                         return;
                                     auto param_name = attribute(param, "name");
                                     auto value      = attribute(param, "value");
                                     if (!param_name || !value)
                                         return;

                                     if (*param_name == "cacheControl")
                                         media.cache_control = toUpper(*value) == "ONLYIFCACHED"
                                                                   ? CacheControl::OnlyIfCached
                                                                   : CacheControl::UseCache;
                                     else if (*param_name == "filename")
                                         media.filename = *value;
                                     else if (*param_name == "logContentId")
                                         media.log_content_id = *value;
                                     else if (media.kind == MediaKind::Widget)
                                         media.query += (media.query.empty() ? "?" : "&") + *param_name + "=" +
                                                        percentEncode(*value);
                                 });
                }
        };
    }

    std::expected<Document, ParseError> parse(std::string_view text)
    {
        if (text.size() > static_cast<std::size_t>(INT_MAX))
            return std::unexpected(ParseError{0, "document too large"});

        std::unique_ptr<xmlParserCtxt, ContextDeleter> context(xmlNewParserCtxt());
        if (!context)
            return std::unexpected(ParseError{0, "out of memory"});

        // NONET: never load external entities or DTDs from the network
        std::unique_ptr<xmlDoc, DocDeleter> doc(
            xmlCtxtReadMemory(context.get(), text.data(), static_cast<int>(text.size()), "index.smil", nullptr,
                              XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING));
        if (!doc)
        {
            const xmlError *error = xmlCtxtGetLastError(context.get());
            if (error == nullptr || error->message == nullptr)
                return std::unexpected(ParseError{0, "invalid XML"});
            return std::unexpected(ParseError{error->line, std::string(trim(error->message))});
        }

        const xmlNode *root = xmlDocGetRootElement(doc.get());
        if (root == nullptr || name(root) != "smil")
            return std::unexpected(ParseError{root ? static_cast<int>(xmlGetLineNo(root)) : 0, "root element is not <smil>"});

        return Parser{}.parse(root);
    }
}
