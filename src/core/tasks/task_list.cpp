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
#include "tasks/task_list.hpp"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <climits>
#include <memory>

namespace garlic::core::tasks
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

        using Document = std::unique_ptr<xmlDoc, DocDeleter>;

        std::string_view name(const xmlNode *node)
        {
            return reinterpret_cast<const char *>(node->name);
        }

        std::string text(const xmlNode *node)
        {
            xmlChar    *value = xmlNodeGetContent(node);
            std::string result(value ? reinterpret_cast<const char *>(value) : "");
            xmlFree(value);
            auto first = result.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
                return {};
            return result.substr(first, result.find_last_not_of(" \t\r\n") - first + 1);
        }

        std::string attribute(const xmlNode *node, const char *attribute_name)
        {
            xmlChar *value = xmlGetNoNsProp(node, reinterpret_cast<const xmlChar *>(attribute_name));
            if (value == nullptr)
                return {};
            std::string result(reinterpret_cast<const char *>(value));
            xmlFree(value);
            return result;
        }

        /// Without network access, errors with line number.
        std::expected<Document, XmlError> read(std::string_view xml)
        {
            if (xml.size() > static_cast<std::size_t>(INT_MAX))
                return std::unexpected(XmlError{0, "document too large"});
            std::unique_ptr<xmlParserCtxt, ContextDeleter> context(xmlNewParserCtxt());
            if (!context)
                return std::unexpected(XmlError{0, "out of memory"});
            Document doc(xmlCtxtReadMemory(context.get(), xml.data(), static_cast<int>(xml.size()), "task.xml", nullptr,
                                           XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING));
            if (!doc)
            {
                const xmlError *error = xmlCtxtGetLastError(context.get());
                return std::unexpected(XmlError{error ? error->line : 0, error && error->message ? error->message : "invalid XML"});
            }
            return doc;
        }

        /// All descendants with this name.
        void find(const xmlNode *parent, std::string_view tag, std::vector<const xmlNode *> &found)
        {
            for (const xmlNode *child = parent->children; child != nullptr; child = child->next)
            {
                if (child->type != XML_ELEMENT_NODE)
                    continue;
                if (name(child) == tag)
                    found.push_back(child);
                find(child, tag, found);
            }
        }

        /// All elements of the document with this name, like QDomDocument::elementsByTagName.
        std::vector<const xmlNode *> elements(const Document &doc, std::string_view tag)
        {
            std::vector<const xmlNode *> found;
            const xmlNode               *root = xmlDocGetRootElement(doc.get());
            if (root == nullptr)
                return found;
            if (name(root) == tag)
                found.push_back(root);
            find(root, tag, found);
            return found;
        }
    }

    std::expected<std::vector<Task>, XmlError> parseTaskList(std::string_view xml)
    {
        auto doc = read(xml);
        if (!doc)
            return std::unexpected(doc.error());

        auto lists = elements(*doc, "timedTaskList");
        std::vector<Task> tasks;
        if (lists.empty())
            return tasks;

        for (const xmlNode *node = lists.front()->children; node != nullptr; node = node->next)
        {
            if (node->type != XML_ELEMENT_NODE)
                continue;
            Task             task;
            std::string_view tag = name(node);
            if (tag == "updateSettings")
                task.kind = Task::Kind::UpdateSettings;
            else if (tag == "firmwareUpdate")
                task.kind = Task::Kind::FirmwareUpdate;
            else if (tag == "shutdownPlayer")
                task.kind = Task::Kind::ShutdownPlayer;
            else if (tag == "applyCommand")
                task.kind = Task::Kind::ApplyCommand;
            else
                continue;
            task.id = attribute(node, "id");

            for (const xmlNode *child = node->children; child != nullptr; child = child->next)
            {
                if (child->type != XML_ELEMENT_NODE)
                    continue;
                std::string_view field = name(child);
                if (field == "sourceURI")
                    task.source_uri = text(child);
                else if (field == "version")
                    task.version = text(child);
                else if (field == "contentType")
                    task.content_type = text(child);
                else if (field == "contentChecksum")
                    task.content_checksum = text(child);
                else if (field == "checksumMethod")
                    task.checksum_method = text(child);
                else if (field == "command" && task.command.empty())
                    task.command = text(child);
            }
            tasks.push_back(std::move(task));
        }
        return tasks;
    }

    std::expected<std::vector<std::pair<std::string, std::string>>, XmlError> parseSettings(std::string_view xml)
    {
        auto doc = read(xml);
        if (!doc)
            return std::unexpected(doc.error());

        auto props = elements(*doc, "prop");
        std::vector<std::pair<std::string, std::string>> settings;
        for (const xmlNode *prop : props)
        {
            std::string value = attribute(prop, "value");
            if (!value.empty())
                settings.emplace_back(attribute(prop, "name"), std::move(value));
        }
        return settings;
    }
}
