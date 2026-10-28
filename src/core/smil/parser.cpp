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
#include <memory>
#include <optional>

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

        class Parser
        {
            public:
                Document parse(const xmlNode *smil)
                {
                    Document document;
                    forEachChild(smil,
                                 [&](const xmlNode *child)
                                 {
                                     if (name(child) == "head")
                                         parseHead(child, document.head);
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
