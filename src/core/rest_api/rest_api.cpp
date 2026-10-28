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
#include "rest_api/rest_api.hpp"

#include "common/md5.hpp"
#include "common/time_format.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <charconv>
#include <chrono>
#include <format>
#include <optional>

namespace garlic::core::rest_api
{
    using httpd::Request;
    using httpd::Response;
    using nlohmann::json;
    using downloader::InventoryState;

    namespace
    {
        // --- answers exactly like the RequestMapper of garlic-lib

        Response respond(const json &body)
        {
            // invalid UTF-8 from a client must not abort: it is replaced
            return {200,
                    "application/json; charset=utf-8",
                    body.dump(-1, ' ', false, json::error_handler_t::replace),
                    {{"Cache-Control", "no-cache,no-store"}, {"Pragma", "no-cache"}}};
        }

        Response respondError(const std::string &text)
        {
            return respond(json{{"error", text}});
        }

        Response accessViolation()
        {
            return {403, "", "{'error':'ACCESSVIOLATION'}", {}};
        }

        Response notFound()
        {
            return {404, "", "The URL is wrong, no such document.", {}};
        }

        /// garlic-lib: respond(response, text) with a text that is not built as JSON, e.g. "" of task/reboot
        Response respondText(std::string text)
        {
            return {200, "application/json; charset=utf-8", std::move(text),
                    {{"Cache-Control", "no-cache,no-store"}, {"Pragma", "no-cache"}}};
        }

        // --- JSON request bodies; no exceptions: types are checked before every access

        std::optional<json> parseObject(const std::string &body)
        {
            json parsed = json::parse(body, nullptr, false);
            if (parsed.is_discarded() || !parsed.is_object())
                return std::nullopt;
            return parsed;
        }

        std::string stringValue(const json &object, const char *key)
        {
            auto it = object.find(key);
            return it != object.end() && it->is_string() ? it->get<std::string>() : std::string{};
        }

        std::int64_t toInt(std::string_view text)
        {
            std::int64_t value = 0;
            std::from_chars(text.data(), text.data() + text.size(), value);
            return value;
        }

        /// garlic-lib read numbers as strings ("10"); numbers (10) are accepted too
        std::int64_t intValue(const json &object, const char *key)
        {
            auto it = object.find(key);
            if (it == object.end())
                return 0;
            if (it->is_number_integer())
                return it->get<std::int64_t>();
            return it->is_string() ? toInt(it->get<std::string>()) : 0;
        }

        bool isJson(const Request &request)
        {
            return request.header("content-type").starts_with("application/json");
        }

        // --- file names like QFileInfo

        /// "abc.def.jpg" → "abc" (QFileInfo::baseName: up to the first dot)
        std::string baseName(std::string_view file_name)
        {
            return std::string(file_name.substr(0, file_name.find('.')));
        }

        /// "http://x/media/a.b.jpg" → "jpg" (QFileInfo::suffix: after the last dot of the last component)
        std::string suffix(std::string_view path)
        {
            std::string_view name = path.substr(path.rfind('/') + 1);
            auto             dot  = name.rfind('.');
            return dot == std::string_view::npos ? std::string{} : std::string(name.substr(dot + 1));
        }

        /// MIME type by extension, instead of QMimeDatabase
        std::string mimeTypeFor(std::string_view ext)
        {
            constexpr std::array<std::pair<std::string_view, std::string_view>, 22> types = {{
                {"jpg", "image/jpeg"},        {"jpeg", "image/jpeg"},     {"png", "image/png"},
                {"gif", "image/gif"},         {"webp", "image/webp"},     {"bmp", "image/bmp"},
                {"svg", "image/svg+xml"},     {"mp4", "video/mp4"},       {"webm", "video/webm"},
                {"mov", "video/quicktime"},   {"mkv", "video/x-matroska"}, {"mp3", "audio/mpeg"},
                {"ogg", "audio/ogg"},         {"wav", "audio/x-wav"},     {"smil", "application/smil+xml"},
                {"xml", "application/xml"},   {"zip", "application/zip"}, {"wgt", "application/widget"},
                {"apk", "application/vnd.android.package-archive"},       {"html", "text/html"},
                {"txt", "text/plain"},        {"pdf", "application/pdf"},
            }};

            std::string lower(ext);
            for (char &c : lower)
                c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);

            for (const auto &[e, type] : types)
            {
                if (e == lower)
                    return std::string(type);
            }
            return "application/octet-stream";
        }

        /// "2026-10-28T03:30:56" with optional zone, as QDateTime::fromString(…, Qt::ISODate) accepts it
        bool isIsoDate(std::string_view text)
        {
            constexpr std::string_view pattern = "dddd-dd-ddTdd:dd:dd";
            if (text.size() < pattern.size())
                return false;
            for (std::size_t i = 0; i < pattern.size(); ++i)
            {
                bool digit = text[i] >= '0' && text[i] <= '9';
                if (pattern[i] == 'd' ? !digit : text[i] != pattern[i])
                    return false;
            }
            return true;
        }

        std::string isoTime(std::chrono::system_clock::time_point time)
        {
            return std::format("{:%FT%T}", std::chrono::floor<std::chrono::seconds>(time));
        }
    }

    RestApi::RestApi(configuration::Configuration &c, database::InventoryTable &i, common::FileSystem &f,
                     const common::Clock &cl, RestActions a)
        : config(c), inventory(i), fs(f), clock(cl), actions(std::move(a))
    {
    }

    void RestApi::registerRoutes(httpd::Router &router)
    {
        // garlic-lib did not check the method for these paths
        router.add("*", "/v2/oauth2/token", [this](const Request &r) { return token(r); });
        router.add("*", "/v2/files/new", [this](const Request &r) { return filesNew(r); });
        router.add("*", "/v2/files/find", [this](const Request &r) { return filesFind(r); });
        router.add("*", "/v2/files/delete", [this](const Request &r) { return filesDelete(r); });
        router.add("*", "/v2/files/{id}", [this](const Request &r) { return fileById(r); });
        router.add("*", "/v2/system/{name}", [this](const Request &r) { return system(r); });
        router.add("*", "/v2/app/{name}", [this](const Request &r) { return app(r); });
        router.add("*", "/v2/task/{name}", [this](const Request &r) { return task(r); });
        router.setFallback([](const Request &) { return notFound(); });
    }

    // ------------------------------------------------------------------ oauth2

    Response RestApi::token(const Request &request)
    {
        std::string grant_type = request.param("grant_type");
        std::string username   = request.param("username");
        std::string password   = request.param("password");
        if (isJson(request))
        {
            auto body = parseObject(request.body);
            if (!body)
                return respondError("No JSON found");
            grant_type = stringValue(*body, "grant_type");
            username   = stringValue(*body, "username");
            password   = stringValue(*body, "password");
        }

        if (grant_type != "password")
            return respondError("wrong grant_type");
        // like garlic-lib: fixed user without password, real credentials are a later topic
        if (username != "admin")
            return respondError("wrong username");
        if (!password.empty())
            return respondError("wrong password");

        // garlic-lib passed its (always empty) members, not the request values
        std::string access_token = config.determineApiAccessToken("", "");
        return respond(json{{"access_token", access_token},
                            {"token_type", "Bearer"},
                            {"expires_in", config.value("api_access_token_expire")}});
    }

    bool RestApi::validToken(const Request &request) const
    {
        std::string token  = request.param("access_token");
        std::string expire = config.value("api_access_token_expire");
        return token == config.apiAccessToken() && isoTime(clock.now()) <= expire;
    }

    // ------------------------------------------------------------------ files

    Response RestApi::filesNew(const Request &request)
    {
        if (!validToken(request))
            return accessViolation();

        auto        upload        = request.files.find("data");
        std::string download_path = request.param("downloadPath");
        if (upload == request.files.end())
            return respondError("Data field missed");
        if (toInt(request.param("fileSize")) == 0)
            return respondError("fileSize field missed");
        if (download_path.empty())
            return respondError("downloadPath field missed");

        std::string id = common::Md5::hex(download_path);
        return respond(storeUpload(request, download_path, id + "." + suffix(download_path), id));
    }

    Response RestApi::filesFind(const Request &request)
    {
        if (!validToken(request))
            return accessViolation();

        std::int64_t max_results = toInt(request.param("maxResults"));
        std::int64_t begin       = toInt(request.param("pageToken"));
        if (isJson(request))
        {
            auto body = parseObject(request.body);
            if (!body)
                return respondError("No JSON found");
            max_results = intValue(*body, "maxResults");
            begin       = intValue(*body, "pageToken");
        }

        auto rows = max_results > 0
                        ? inventory.findPaginated(static_cast<int>(max_results), static_cast<int>(begin))
                        : inventory.findAll();

        json items = json::array();
        for (const auto &row : rows)
            items.push_back(fileInfo(baseName(row.file_name)));

        return respond(json{{"items", items}, {"nextPageToken", max_results + begin}});
    }

    Response RestApi::filesDelete(const Request &request)
    {
        if (!validToken(request))
            return accessViolation();

        std::string id = request.param("id");
        if (isJson(request))
        {
            auto body = parseObject(request.body);
            if (!body)
                return respondError("No JSON found");
            id = stringValue(*body, "id");
        }

        auto row = inventory.findByCacheBaseName(id);
        if (!row)
            return respondError("Record not found: " + id);
        if (!fs.remove(cachePath(row->file_name)))
            return respondError("delete failed: " + id);

        inventory.deleteByCacheName(row->file_name);
        return respond(json{{"success", id + " deleted"}});
    }

    Response RestApi::fileById(const Request &request)
    {
        if (!validToken(request))
            return accessViolation();

        const std::string &id = request.path_params.at("id");
        if (request.method == "GET")
            return respond(fileInfo(id));

        if (request.method != "POST")
            return {200, "application/json; charset=utf-8", "", {}}; // garlic-lib answered other methods empty

        std::string download_path = request.param("downloadPath");
        if (download_path.empty())
            return respondError("downloadPath field missed");

        auto row = inventory.findByCacheBaseName(id);
        if (!row)
            return respondError("Record not found: " + id);
        if (row->url != download_path)
            return respondError("Record not identical to downloadPath" + id);

        return respond(storeUpload(request, download_path, row->file_name, baseName(row->file_name)));
    }

    std::string RestApi::cachePath(std::string_view cache_name) const
    {
        const std::string &dir = config.cacheDir();
        if (dir.empty() || dir.back() == '/')
            return dir + std::string(cache_name);
        return dir + "/" + std::string(cache_name);
    }

    /// File entry as JSON, or the error object if the id is unknown (garlic-lib: Files::createObject)
    json RestApi::fileInfo(std::string_view id) const
    {
        auto row = inventory.findByCacheBaseName(id);
        if (!row)
            return json{{"error", "Record not found: " + std::string(id)}};

        std::string path     = cachePath(row->file_name);
        auto        modified = fs.lastModified(path);
        std::string last     = row->last_update;
        if (last.ends_with('Z'))
            last.pop_back(); // garlic-lib wrote ISO dates without zone

        return {
            {"id", baseName(row->file_name)},
            {"etag", baseName(row->file_name)},
            {"mimeType", row->content_type},
            {"createdDate", modified ? isoTime(*modified) : ""},
            {"modifiedDate", last},
            {"downloadPath", path},
            {"fileSize", std::to_string(row->content_length)}, // a string in garlic-lib
            {"transferredSize", fs.fileSize(path).value_or(0)},
            {"completed", row->state == InventoryState::Complete},
        };
    }

    /// Moves the uploaded file into the cache and updates the inventory (garlic-lib: createNew/modifyByID)
    json RestApi::storeUpload(const Request &request, std::string download_path, std::string cache_name,
                                     std::string_view id)
    {
        std::string path      = cachePath(cache_name);
        auto        file_size = static_cast<std::uint64_t>(toInt(request.param("fileSize")));

        if (auto upload = request.files.find("data"); upload != request.files.end())
            (void)fs.rename(upload->second.path, path); // replaces an existing file

        auto        stored   = fs.fileSize(path).value_or(0);
        auto        modified = fs.lastModified(path);
        std::string created  = modified ? isoTime(*modified) : "";

        downloader::InventoryEntry entry;
        entry.url            = std::move(download_path);
        entry.file_name      = std::move(cache_name);
        entry.content_type   = request.param("mimeType");
        entry.content_length = file_size;
        entry.state          = stored == file_size ? InventoryState::Complete : InventoryState::Transfer;
        if (entry.content_type.empty())
            entry.content_type = mimeTypeFor(suffix(entry.file_name));

        std::string modified_date = request.param("modifiedDate");
        inventory.save(entry, isIsoDate(modified_date) ? modified_date : created);

        return {{"downloadPath", entry.url},
                {"fileSize", file_size},
                {"id", std::string(id)},
                {"createdDate", created},
                {"transferredSize", stored},
                {"completed", entry.state == InventoryState::Complete}};
    }

    // ------------------------------------------------------------------ system (garlic-lib: SystemInfo)

    /// Token first, then the name: an unknown name with a valid token is 404, without token 403 (RequestMapper).
    Response RestApi::system(const Request &request)
    {
        if (!validToken(request))
            return accessViolation();

        std::string name = request.path_params.at("name");
        std::string os(configuration::Configuration::os());
        if (name == "firmwareInfo")
            return respond(json{{"firmwareVersion", config.version()}, {"family", "garlic-player-" + os}});
        if (name == "modelInfo")
            return respond(json{{"modelDescription", ""},
                                {"modelName", "garlic-player"},
                                {"modelURL", ""},
                                {"manufacturer", "Sagiadinos"},
                                {"licenseModel", os},
                                {"PCBRevision", ""},
                                {"manufacturerURL", "https://garlic-player.com"},
                                {"PCB", "garlic-player"},
                                {"options", ""}});
        return notFound();
    }

    // ------------------------------------------------------------------ app (garlic-lib: App)

    Response RestApi::app(const Request &request)
    {
        if (!validToken(request))
            return accessViolation();

        std::string name = request.path_params.at("name");
        if (name == "start") // change the index URI and switch to it
            return appStart(request, false);
        if (name == "exec") // play the URI, but keep the index URI
            return appStart(request, true);
        if (name == "switch") // play the index URI again
        {
            std::string mode = request.param("mode");
            if (isJson(request))
            {
                auto body = parseObject(request.body);
                if (!body)
                    return respondError("No JSON found");
                mode = stringValue(*body, "mode");
            }
            if (mode != "start")
                return respondError("wrong mode");
            if (actions.reloadIndex)
                actions.reloadIndex();
            std::string uri = config.indexUri();
            return respond(json{{"uri", uri}, {"packageName", ""}, {"className", ""}, {"data", uri}});
        }
        return notFound();
    }

    /// garlic-lib: App::start / App::exec - the parameters Action and Type were read but not used
    Response RestApi::appStart(const Request &request, bool temporary)
    {
        std::string uri          = request.param("uri");
        std::string package_name = request.param("packageName");
        std::string class_name   = request.param("className");
        if (isJson(request))
        {
            auto body = parseObject(request.body);
            if (!body)
                return respondError("No JSON found");
            uri          = stringValue(*body, "uri");
            package_name = stringValue(*body, "packageName");
            class_name   = stringValue(*body, "className");
        }

        auto valid = configuration::Configuration::validateIndexUri(uri);
        if (!valid)
            return respondError(valid.error());

        if (temporary)
        {
            if (actions.playTemporary)
                actions.playTemporary(*valid);
        }
        else
        {
            config.setIndexUri(*valid);
            if (actions.reloadIndex)
                actions.reloadIndex();
        }
        return respond(json{{"uri", uri}, {"packageName", package_name}, {"className", class_name}, {"data", uri}});
    }

    // ------------------------------------------------------------------ task (garlic-lib: Task)

    Response RestApi::task(const Request &request)
    {
        if (!validToken(request))
            return accessViolation();

        std::string name = request.path_params.at("name");
        if (name == "notify")
        {
            std::string event = request.param("smilEvent");
            if (isJson(request))
            {
                auto body = parseObject(request.body);
                if (!body)
                    return respondError("No JSON found");
                event = stringValue(*body, "smilEvent");
            }
            if (event.empty())
                return respondError(" No SmilEvent exists"); // with the space of garlic-lib
            if (actions.notify)
                actions.notify(event);
            return respond(json::object());
        }
        if (name == "reboot")
        {
            // garlic-lib: "httpd: " + QDateTime::currentDateTime().toString(Qt::ISODate)
            if (actions.reboot)
                actions.reboot(std::format("httpd: {:%Y-%m-%dT%H:%M:%S}", common::toLocalTime(clock.now())));
            return respondText("");
        }
        if (name == "screenshot")
            return screenshot();
        return notFound();
    }

    /**
     * The frontend saves the screenshot; the answer waits for it (garlic-lib waited one second in the event loop and
     * hoped). The file is named like in garlic-lib: <log dir>/yyyyMMddhhmmss.jpg.
     */
    Response RestApi::screenshot()
    {
        std::string path = config.logDir() + "/" +
                           std::format("{:%Y%m%d%H%M%S}", common::toLocalTime(clock.now())) + ".jpg";
        if (!actions.screenshot)
            return screenshotFile(path);

        auto pending = std::make_shared<httpd::PendingResponse>();
        screenshots.push_back({path, pending, clock.monotonic() + screenshot_timeout});
        actions.screenshot(path);
        return Response::later(pending);
    }

    void RestApi::screenshotTaken(std::string_view path)
    {
        std::erase_if(screenshots,
                      [&](Screenshot &s)
                      {
                          if (s.path != path)
                              return false;
                          s.pending->complete(screenshotFile(s.path));
                          return true;
                      });
    }

    void RestApi::poll()
    {
        auto now = clock.monotonic();
        std::erase_if(screenshots,
                      [&](Screenshot &s)
                      {
                          if (now < s.deadline)
                              return false;
                          s.pending->complete(screenshotFile(s.path)); // perhaps it is there anyway
                          return true;
                      });
    }

    std::optional<std::chrono::steady_clock::time_point> RestApi::nextDeadline() const
    {
        std::optional<std::chrono::steady_clock::time_point> next;
        for (const auto &s : screenshots)
            if (!next || s.deadline < *next)
                next = s.deadline;
        return next;
    }

    /// garlic-lib: RequestMapper::respondImage
    Response RestApi::screenshotFile(const std::string &path) const
    {
        if (!fs.exists(path))
            return {200, "", "file not created.", {}};
        auto data = fs.readFile(path);
        if (!data)
            return {200, "", "file not readable.", {}};
        return {200, "image/jpeg", std::move(*data), {}};
    }
}
