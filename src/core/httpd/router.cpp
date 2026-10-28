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
#include "httpd/router.hpp"

#include <algorithm>

namespace garlic::core::httpd
{
    namespace
    {
        std::vector<std::string> split(std::string_view path)
        {
            std::vector<std::string> segments;
            while (!path.empty())
            {
                auto slash = path.find('/');
                if (slash != 0)
                    segments.emplace_back(path.substr(0, slash));
                if (slash == std::string_view::npos)
                    break;
                path.remove_prefix(slash + 1);
            }
            return segments;
        }

        bool isPlaceholder(std::string_view segment)
        {
            return segment.size() > 2 && segment.front() == '{' && segment.back() == '}';
        }

        std::string toLower(std::string_view text)
        {
            std::string result(text);
            for (char &c : result)
            {
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
            }
            return result;
        }
    }

    std::string Request::param(std::string_view name) const
    {
        if (auto it = query.find(std::string(name)); it != query.end())
            return it->second;
        if (auto it = form.find(std::string(name)); it != form.end())
            return it->second;
        return {};
    }

    std::string Request::header(std::string_view name) const
    {
        auto it = headers.find(toLower(name));
        return it == headers.end() ? std::string{} : it->second;
    }

    void Router::add(std::string method, std::string pattern, Handler handler)
    {
        routes.push_back({std::move(method), split(pattern), std::move(handler)});
    }

    Response Router::handle(Request &request) const
    {
        auto segments    = split(request.path);
        bool path_exists = false;

        for (const Route &route : routes)
        {
            if (route.segments.size() != segments.size())
                continue;

            std::map<std::string, std::string> params;
            bool                               match = true;
            for (std::size_t i = 0; i < segments.size() && match; ++i)
            {
                const std::string &part = route.segments[i];
                if (isPlaceholder(part))
                    params[part.substr(1, part.size() - 2)] = segments[i];
                else
                    match = part == segments[i];
            }
            if (!match)
                continue;

            path_exists = true;
            if (route.method != request.method)
                continue;

            request.path_params = std::move(params);
            return route.handler(request);
        }

        if (path_exists)
            return Response::json(405, R"({"error":"method not allowed"})");
        return Response::json(404, R"({"error":"not found"})");
    }
}
