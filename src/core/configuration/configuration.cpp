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
#include "configuration/configuration.hpp"

#include "common/md5.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <format>
#include <random>

#if defined(__APPLE__)
    #include <TargetConditionals.h>
#endif

namespace garlic::core::configuration
{
    namespace
    {
        /// Random UUID version 4, lowercase like QUuid.
        std::string newUuid()
        {
            std::random_device                 random;
            std::array<std::uint8_t, 16>       bytes;
            std::uniform_int_distribution<int> byte(0, 255);
            for (auto &b : bytes)
                b = static_cast<std::uint8_t>(byte(random));

            bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0F) | 0x40); // version 4
            bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3F) | 0x80); // variant RFC 4122

            std::string uuid;
            for (std::size_t i = 0; i < bytes.size(); ++i)
            {
                if (i == 4 || i == 6 || i == 8 || i == 10)
                    uuid.push_back('-');
                uuid += std::format("{:02x}", bytes[i]);
            }
            return uuid;
        }

        bool hasScheme(std::string_view uri)
        {
            auto colon = uri.find(':');
            return colon != std::string_view::npos && colon > 0 &&
                   uri.substr(0, colon).find_first_of("/?#") == std::string_view::npos;
        }

        /// Number from the ini, the default if the key is missing or no valid number.
        template <typename T> T number(const std::string &text, T fallback)
        {
            T value{};
            auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            return error == std::errc{} && end == text.data() + text.size() ? value : fallback;
        }

        std::string upper(std::string text)
        {
            for (char &c : text)
            {
                if (c >= 'a' && c <= 'z')
                    c = static_cast<char>(c - 'a' + 'A');
            }
            return text;
        }
    }

    std::string Configuration::createUuid()
    {
        return newUuid();
    }

    Configuration::Configuration(common::FileSystem &f, const common::Clock &c, std::string path)
        : fs(f), clock(c), ini_path(std::move(path))
    {
    }

    std::expected<void, std::string> Configuration::load()
    {
        auto text = fs.readFile(ini_path);
        if (text)
            ini = IniFile::parse(*text);
        else if (text.error().code != common::FileSystemErrorCode::NotFound)
            return std::unexpected(text.error().message);

        if (uuid().empty())
            setValue("uuid", createUuid());

        // like garlic-lib: without a name the player is called after the last part of its uuid
        if (playerName().empty())
        {
            std::string id = uuid();
            setPlayerName(id.substr(id.rfind('-') + 1));
        }
        return {};
    }

    std::string Configuration::value(std::string_view key) const
    {
        return ini.value(key).value_or("");
    }

    void Configuration::setValue(std::string key, std::string new_value)
    {
        if (ini.value(key) == new_value)
            return;

        ini.setValue(std::move(key), std::move(new_value));
        save();
    }

    void Configuration::determineIndexUri(std::string_view command_line_uri)
    {
        if (!command_line_uri.empty())
            setIndexUri(std::string(command_line_uri));
    }

    std::string Configuration::indexPath() const
    {
        std::string uri = indexUri();
        if (uri.starts_with("http") || uri.starts_with("ftp"))
        {
            uri = uri.substr(0, uri.find_first_of("?#"));

            auto host  = uri.find("://");
            auto slash = uri.rfind('/');
            if (host == std::string::npos || slash < host + 3)
                return uri + "/";
            return uri.substr(0, slash + 1);
        }

        auto slash = uri.rfind('/');
        return slash == std::string::npos ? "./" : uri.substr(0, slash + 1);
    }

    std::expected<std::string, std::string> Configuration::validateIndexUri(std::string_view uri)
    {
        if (uri.empty())
            return std::unexpected("A content-url is necessary");

        if (hasScheme(uri))
        {
            auto colon = uri.find(':');
            if (uri.substr(colon + 1, 2) != "//")
                return std::unexpected("Url scheme is not valid! Use " + std::string(uri.substr(0, colon)) +
                                       "://domain.tld");

            if (uri.substr(0, colon) == "file")
                return std::string(uri.substr(colon + 3)); // file:///path → /path
            return std::string(uri);
        }

        if (!uri.starts_with('/'))
            return std::unexpected("Relative path to SMIL-Index is not allowed! Use /path/to/index, "
                                   "http://domain.tld, https://domain.tld, or file://path/to/index");
        return std::string(uri);
    }

    std::string Configuration::determineApiAccessToken(std::string_view username, std::string_view password)
    {
        // ISO strings of the same format sort like the times they stand for
        auto        now_seconds = std::chrono::floor<std::chrono::seconds>(clock.now());
        std::string now         = std::format("{:%FT%T}", now_seconds);
        if (now < value("api_access_token_expire") && !apiAccessToken().empty())
            return apiAccessToken();

        std::string expire = std::format("{:%FT%T}", now_seconds + std::chrono::days{1});
        std::string token  = upper(common::Md5::hex(std::string(username) + std::string(password) + expire));
        setValue("api_access_token_expire", expire);
        setValue("api_access_token", token);
        return token;
    }

    httpd::HttpdSettings Configuration::httpd() const
    {
        httpd::HttpdSettings s;
        std::string          enabled = value("httpd/enabled");
        s.enabled                    = enabled != "false" && enabled != "0";
        s.bind_address               = value("httpd/host");
        s.port                       = number(value("httpd/port"), s.port);
        s.max_request_size           = number(value("httpd/maxRequestSize"), s.max_request_size);
        s.max_multipart_size         = number(value("httpd/maxMultiPartSize"), s.max_multipart_size);
        s.upload_dir                 = cache_dir;

        // milliseconds in the ini (QtWebApp), whole seconds for the server, at least one
        auto timeout_ms = number<std::int64_t>(value("httpd/readTimeout"), 60000);
        s.read_timeout  = std::chrono::seconds{std::max<std::int64_t>(1, (timeout_ms + 999) / 1000)};
        return s;
    }

    std::string_view Configuration::os()
    {
#if defined(__ANDROID__)
        return "android";
#elif defined(__APPLE__) && TARGET_OS_IPHONE
        return "iOS";
#elif defined(__APPLE__)
        return "macOS";
#elif defined(__EMSCRIPTEN__)
        return "wasm";
#elif defined(ESP_PLATFORM)
        return "esp32";
#elif defined(__linux__)
        return "linux";
#elif defined(_WIN32)
        return "windows";
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
        return "BSD";
#else
        return "unknown";
#endif
    }

    std::string Configuration::userAgent() const
    {
        return "GAPI/1.0 (UUID:" + uuid() + "; NAME:" + playerName() + ") garlic-" + std::string(os()) + "/" + version +
               " (MODEL:Garlic)";
    }

    void Configuration::save()
    {
        // Write to a temporary file and rename it: a power cut while writing never leaves a broken ini behind.
        std::string temp = ini_path + ".tmp";
        std::string text = ini.serialize();

        if (auto slash = ini_path.rfind('/'); slash != std::string::npos && slash > 0)
            (void)fs.createDirectories(ini_path.substr(0, slash));

        auto file = fs.openForWrite(temp, common::WriteMode::Truncate);
        if (!file)
        {
            last_error = file.error().message;
            return;
        }

        auto written = (*file)->write(std::as_bytes(std::span(text.data(), text.size())));
        auto closed  = (*file)->close();
        if (!written || !closed)
        {
            last_error = !written ? written.error().message : closed.error().message;
            (void)fs.remove(temp);
            return;
        }

        if (auto renamed = fs.rename(temp, ini_path); !renamed)
            last_error = renamed.error().message;
    }
}
