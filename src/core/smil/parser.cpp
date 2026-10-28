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

#include "common/time_format.hpp"

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

        bool isDigit(char c)
        {
            return c >= '0' && c <= '9';
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
            else if (!value.empty() && !isDigit(value.front()) && value.front() != '.')
            {
                // like garlic-lib (ClockValue::separateSign): one leading character that is no digit is dropped,
                // e.g. a typo like dur="´1s"; a UTF-8 character can have several bytes
                std::size_t length = 1;
                while (length < value.size() && (static_cast<unsigned char>(value[length]) & 0xC0) == 0x80)
                    ++length;
                value.remove_prefix(length);
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

        // ------------------------------------------------------------------ triggers

        bool isLetter(char c)
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        }

        /// "" → 0, "+2s" → 2 s, "-1.5s" → -1.5 s; anything else is no offset.
        std::optional<std::chrono::milliseconds> parseTriggerOffset(std::string_view rest)
        {
            rest = trim(rest);
            if (rest.empty())
                return std::chrono::milliseconds(0);
            if (rest.front() != '+' && rest.front() != '-')
                return std::nullopt;
            return parseClockValue(rest);
        }

        /// The text between "name(" and ")" and the offset after it: "accesskey(a)+2s" → "a", 2 s.
        std::optional<std::pair<std::string_view, std::chrono::milliseconds>> parseCall(std::string_view value,
                                                                                         std::string_view function)
        {
            std::string_view rest = value.substr(function.size() + 1); // after "name("
            auto             close = rest.find(')');
            if (close == std::string_view::npos)
                return std::nullopt;
            auto offset = parseTriggerOffset(rest.substr(close + 1));
            if (!offset)
                return std::nullopt;
            return std::pair{rest.substr(0, close), *offset};
        }

        /**
         * "img1.begin+2s", "img1.end", "img1.activateEvent-1s", garlic-lib: SyncBase and Event.
         * The id ends at the "." before a known symbol, so ids may contain "-" and ".".
         */
        std::optional<TimeCondition> parseSyncBaseOrEvent(std::string_view value)
        {
            struct Symbol
            {
                std::string_view name;
                TimeCondition    condition;
            };
            static const Symbol symbols[] = {
                {"beginEvent", Event{{}, Event::Type::BeginEvent, {}}},
                {"endEvent", Event{{}, Event::Type::EndEvent, {}}},
                {"activateEvent", Event{{}, Event::Type::ActivateEvent, {}}},
                {"click", Event{{}, Event::Type::Click, {}}},
                {"begin", SyncBase{{}, SyncBase::Edge::Begin, {}}},
                {"end", SyncBase{{}, SyncBase::Edge::End, {}}},
            };

            for (auto dot = value.find('.'); dot != std::string_view::npos; dot = value.find('.', dot + 1))
            {
                std::string_view id    = value.substr(0, dot);
                std::string_view after = value.substr(dot + 1);
                for (const Symbol &symbol : symbols)
                {
                    if (id.empty() || !after.starts_with(symbol.name))
                        continue;
                    auto offset = parseTriggerOffset(after.substr(symbol.name.size()));
                    if (!offset)
                        continue;

                    TimeCondition condition = symbol.condition;
                    std::visit(
                        [&](auto &c)
                        {
                            if constexpr (requires { c.id; })
                            {
                                c.id     = std::string(id);
                                c.offset = *offset;
                            }
                        },
                        condition);
                    return condition;
                }
            }
            return std::nullopt;
        }

        /// "P1Y2M3W4DT5H6M7S", garlic-lib: IsoDate::analysePeriods. Reading stops at the first unknown part.
        Period parsePeriod(std::string_view value)
        {
            Period period;
            if (!value.starts_with('P'))
                return period;
            value.remove_prefix(1);

            bool time = false;
            while (!value.empty())
            {
                if (value.front() == 'T')
                {
                    time = true;
                    value.remove_prefix(1);
                    continue;
                }
                int  number = 0;
                auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), number);
                if (error != std::errc() || end == value.data() + value.size())
                    break;
                char unit = *end;
                value.remove_prefix(static_cast<std::size_t>(end - value.data()) + 1);

                if (!time && unit == 'Y')
                    period.years = number;
                else if (!time && unit == 'M')
                    period.months = number;
                else if (!time && unit == 'W')
                    period.days += number * 7;
                else if (!time && unit == 'D')
                    period.days += number;
                else if (time && unit == 'H')
                    period.time += std::chrono::hours(number);
                else if (time && unit == 'M')
                    period.time += std::chrono::minutes(number);
                else if (time && unit == 'S')
                    period.time += std::chrono::seconds(number);
                else
                    break;
            }
            return period;
        }

        bool hasPeriod(const Period &period)
        {
            return period.years != 0 || period.months != 0 || period.days != 0 || period.time.count() != 0;
        }

        /// "yyyy-MM-dd"; invalid dates become 2000-01-01 like in garlic-lib.
        std::chrono::year_month_day parseDate(std::string_view value)
        {
            using namespace std::chrono;
            auto parts = split(value, '-');
            if (parts.size() == 3 && parts[0].size() == 4 && parts[1].size() == 2 && parts[2].size() == 2)
            {
                year_month_day date{year{static_cast<int>(toNumber(parts[0]))},
                                    month{static_cast<unsigned>(toNumber(parts[1]))},
                                    day{static_cast<unsigned>(toNumber(parts[2]))}};
                if (date.ok())
                    return date;
            }
            return year{2000} / January / 1;
        }

        /**
         * "2026-01-05+w3": the next Wednesday after the date, "-w3" the one before, garlic-lib: IsoDate.
         * Weekdays 1 = Monday … 7 = Sunday. "w3" without sign counts as "+w3".
         */
        std::chrono::sys_days parseDateWithWeekday(std::string_view value)
        {
            using namespace std::chrono;
            auto w = value.find('w');
            if (w == std::string_view::npos)
                return sys_days{parseDate(value)};

            char op      = w > 0 ? value[w - 1] : '+';
            int  weekday = w + 1 < value.size() ? static_cast<int>(toNumber(value.substr(w + 1, 1))) : 0;
            auto date    = parseDate(value.substr(0, op == '+' || op == '-' ? w - 1 : w));
            if (weekday < 1 || weekday > 7)
                return sys_days{date};

            int diff = weekday - static_cast<int>(std::chrono::weekday{sys_days{date}}.iso_encoding());
            if (op == '-')
                diff = diff >= 0 ? diff - 7 : diff;
            else
                diff = diff <= 0 ? diff + 7 : diff;
            return sys_days{date} + days{diff};
        }

        /// "HH", "HH:mm", "HH:mm:ss"; invalid times become 00:00:00 like in garlic-lib.
        std::chrono::seconds parseTimeOfDay(std::string_view value)
        {
            auto parts = split(value, ':');
            if (parts.empty() || parts.size() > 3)
                return std::chrono::seconds(0);
            int values[3] = {0, 0, 0};
            for (std::size_t i = 0; i < parts.size(); ++i)
            {
                if (parts[i].size() != 2)
                    return std::chrono::seconds(0);
                values[i] = static_cast<int>(toNumber(parts[i]));
            }
            if (values[0] > 23 || values[1] > 59 || values[2] > 59)
                return std::chrono::seconds(0);
            return std::chrono::hours(values[0]) + std::chrono::minutes(values[1]) + std::chrono::seconds(values[2]);
        }

        /**
         * "2026-01-05+w3T08:00:00+01:00" → local time. Without zone the time is local time, with "Z" or "±hh:mm"
         * it is converted to local time.
         */
        std::chrono::local_seconds parseWallclockDate(std::string_view value)
        {
            using namespace std::chrono;
            auto             t    = value.find('T');
            sys_days         date = parseDateWithWeekday(value.substr(0, t));
            std::string_view time = t == std::string_view::npos ? std::string_view() : value.substr(t + 1);

            std::optional<seconds> zone; // offset east of UTC
            if (time.ends_with('Z'))
            {
                zone = seconds(0);
                time.remove_suffix(1);
            }
            else if (auto sign = time.find_first_of("+-"); sign != std::string_view::npos)
            {
                auto parts = split(time.substr(sign + 1), ':');
                auto zoned = hours(static_cast<int>(toNumber(parts[0]))) +
                             minutes(parts.size() > 1 ? static_cast<int>(toNumber(parts[1])) : 0);
                zone       = time[sign] == '-' ? -zoned : zoned;
                time       = time.substr(0, sign);
            }

            seconds time_of_day = time.empty() ? seconds(0) : parseTimeOfDay(time);
            if (!zone)
                return local_days{date.time_since_epoch()} + time_of_day;
            return common::toLocalTime(sys_seconds{date + time_of_day - *zone});
        }

        /// "R/2026-01-01T08:00/P1D", "R5/…", "2026-01-01T08:00", "2026-01-01T08:00/P1D", garlic-lib: WallClock
        Wallclock parseWallclock(std::string_view value)
        {
            Wallclock wallclock;
            auto      parts = split(trim(value), '/');
            std::size_t date_index = 0;
            if (!parts.empty() && parts[0].starts_with('R'))
            {
                wallclock.repeats = parts[0].size() == 1 ? -1 : static_cast<int>(toNumber(parts[0].substr(1)));
                date_index        = 1;
            }
            if (date_index < parts.size())
                wallclock.start = parseWallclockDate(parts[date_index]);
            if (date_index + 1 < parts.size())
                wallclock.period = parsePeriod(parts[date_index + 1]);
            if (!hasPeriod(wallclock.period)) // GitHub issue #31
                wallclock.repeats = 0;
            return wallclock;
        }

        /**
         * One value of a begin/end list, garlic-lib: EnhancedTimer::parseOneValue. Same order as there:
         *   indefinite | wallclock(…) | offset | accesskey(…) | notify(…) | id.begin, id.end | id.event
         * Unreadable values give nullopt.
         */
        std::optional<TimeCondition> parseTimeCondition(std::string_view v, ContainerKind parent)
        {
            if (v.contains("indefinite"))
                return Indefinite{};
            if (v.starts_with("wallclock(") && v.ends_with(')'))
            {
                if (parent == ContainerKind::Seq) // children of a seq cannot have wallclocks
                    return std::nullopt;
                return parseWallclock(v.substr(10, v.size() - 11));
            }
            if (v.front() == '+' || v.front() == '-' || isDigit(v.front()))
                return Offset{parseClockValue(v)};
            if (v.starts_with("accesskey("))
            {
                auto call = parseCall(v, "accesskey");
                if (!call || call->first.size() != 1)
                    return std::nullopt;
                char key = call->first.front();
                if (!isDigit(key) && !isLetter(key))
                    return std::nullopt;
                if (key >= 'A' && key <= 'Z')
                    key = static_cast<char>(key - 'A' + 'a');
                return AccessKey{key, call->second};
            }
            if (v.starts_with("notify("))
            {
                auto call = parseCall(v, "notify");
                if (!call || call->first.empty())
                    return std::nullopt;
                return Notify{std::string(call->first), call->second};
            }
            if (isLetter(v.front()))
                return parseSyncBaseOrEvent(v);
            return std::nullopt;
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
                auto condition = parseTimeCondition(v, parent);
                if (!condition)
                    continue;
                if (std::holds_alternative<Indefinite>(*condition) && values.size() > 1)
                    continue;
                conditions.push_back(std::move(*condition));
            }
            return conditions;
        }

        /// <… begin end dur repeatCount restart>, garlic-lib: BaseTimings::parseTimingAttributes
        Timing parseTiming(const xmlNode *node, ContainerKind parent)
        {
            Timing timing;
            if (auto begin = attribute(node, "begin"))
                timing.begin = parseTimeList(*begin, parent);
            if (timing.begin.empty()) // missing or not readable; in an excl only a trigger starts an element
            {
                if (parent == ContainerKind::Excl || parent == ContainerKind::PriorityClass)
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
                    return generateId(name(node), node);
                }

                std::string generateId(std::string_view tag, const xmlNode *node)
                {
                    return std::string(tag) + "_" + std::to_string(xmlGetLineNo(node)) + "_" +
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
                 * <body>, <seq>, <par>, <excl>: the container and its children, nested containers recursively.
                 * parent is the kind of the surrounding container, it decides the defaults of begin.
                 * garlic-lib: TBody, TSeq, TPar, TExcl
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
                    if (kind == ContainerKind::Par || kind == ContainerKind::Excl)
                        parseEndSync(node, container);

                    if (kind == ContainerKind::Excl)
                        parsePriorityClasses(node, container, head);
                    else
                        parseChildren(node, container, head);
                    return container;
                }

                /// The children of node into container: media and nested containers.
                void parseChildren(const xmlNode *node, Container &container, const Head &head)
                {
                    bool first = true;
                    forEachChild(node,
                                 [&](const xmlNode *child)
                                 {
                                     std::string_view tag  = name(child);
                                     ContainerKind    kind = container.kind; // parent of the child
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
                                     else if (tag == "excl")
                                         container.children.emplace_back(
                                             parseContainer(child, ContainerKind::Excl, kind, head));
                                     else if (auto media_kind = mediaKind(child))
                                         container.children.emplace_back(
                                             parseMedia(child, *media_kind, kind, head));
                                     // anything else, also a priorityClass outside of an excl, is ignored
                                     first = false;
                                 });
                }

                /**
                 * The children of an excl are always priorityClasses, the first one has the highest priority:
                 *
                 *   <excl>                              <excl>
                 *     <priorityClass peers="pause">       <img …/>     no priorityClass: all children form
                 *       <img …/>                          <seq …/>     one priorityClass with the defaults
                 *     </priorityClass>                  </excl>
                 *     <img …/>      ignored: mixed with priorityClasses
                 *   </excl>
                 *
                 * garlic-lib: TExcl::traverseChilds
                 */
                void parsePriorityClasses(const xmlNode *excl, Container &container, const Head &head)
                {
                    std::vector<const xmlNode *> classes;
                    forEachChild(excl,
                                 [&](const xmlNode *child)
                                 {
                                     if (name(child) == "priorityClass")
                                         classes.push_back(child);
                                 });

                    if (classes.empty())
                    {
                        Container priority_class;
                        priority_class.kind = ContainerKind::PriorityClass;
                        priority_class.id   = generateId("priorityClass", excl);
                        parseChildren(excl, priority_class, head);
                        container.children.emplace_back(std::move(priority_class));
                        return;
                    }
                    for (const xmlNode *element : classes)
                        container.children.emplace_back(parsePriorityClass(element, head));
                }

                /**
                 * <priorityClass peers="stop" higher="pause" lower="defer">, garlic-lib: TPriorityClass.
                 * No timing; invalid values become the default. A priorityClass inside a priorityClass is ignored.
                 */
                Container parsePriorityClass(const xmlNode *node, const Head &head)
                {
                    Container priority_class;
                    priority_class.kind = ContainerKind::PriorityClass;
                    priority_class.id   = parseId(node);
                    readAttribute(node, "title", priority_class.title);

                    auto value = [&](const char *attribute_name) { return attribute(node, attribute_name).value_or(""); };

                    std::string peers = value("peers");
                    if (peers == "pause")
                        priority_class.peers = Interrupt::Pause;
                    else if (peers == "defer")
                        priority_class.peers = Interrupt::Defer;
                    else if (peers == "never")
                        priority_class.peers = Interrupt::Never;

                    if (value("higher") == "stop")
                        priority_class.higher = Interrupt::Stop;
                    if (value("lower") == "never")
                        priority_class.lower = Interrupt::Never;

                    parseChildren(node, priority_class, head);
                    return priority_class;
                }

                /**
                 * <par|excl endsync="last|first|<id>">, garlic-lib: TPar/TExcl::interruptByEndSync.
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
