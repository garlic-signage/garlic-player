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
#include "httpd/mhd_server.hpp"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
#endif

#include <microhttpd.h>

#include <memory>
#include <vector>

namespace garlic::core::httpd
{
    struct MhdServer::Connection
    {
        MhdServer             *server = nullptr;
        Request                request;
        MHD_PostProcessor     *post      = nullptr;
        bool                   multipart = false;
        bool                   too_large = false;
        bool                   bad_form  = false;
        std::uint64_t          received  = 0;
        std::string            write_error;
        std::map<std::string, std::unique_ptr<common::WritableFile>> open_files;

        ~Connection()
        {
            if (post != nullptr)
                MHD_destroy_post_processor(post);
            open_files.clear();

            // uploads the handler did not take over (rename) are removed
            for (const auto &[name, file] : request.files)
                (void)server->fs.remove(file.path);
        }
    };

    // gives the C callbacks access to the private members
    struct ConnectionAccess
    {
        static std::string nextUploadPath(MhdServer &s)
        {
            return s.settings.upload_dir + "/upload-" + std::to_string(++s.upload_count) + ".tmp";
        }
        static common::FileSystem &fs(MhdServer &s) { return s.fs; }
        static const Router       &router(MhdServer &s) { return s.router; }
        static const HttpdSettings &settings(MhdServer &s) { return s.settings; }
    };

    namespace
    {
        using Connection = MhdServer::Connection;

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

        MHD_Result collectValue(void *cls, MHD_ValueKind kind, const char *key, const char *value)
        {
            auto *request = static_cast<Request *>(cls);
            if (key == nullptr)
                return MHD_YES;

            if (kind == MHD_HEADER_KIND)
                request->headers[toLower(key)] = value != nullptr ? value : "";
            else if (kind == MHD_GET_ARGUMENT_KIND)
                request->query[key] = value != nullptr ? value : "";
            return MHD_YES;
        }

        // form fields and file parts of url-encoded and multipart bodies, in pieces
        MHD_Result onPostData(void *cls, MHD_ValueKind, const char *key, const char *filename, const char *content_type,
                              const char *, const char *data, uint64_t, size_t size)
        {
            auto &c = *static_cast<Connection *>(cls);
            if (key == nullptr)
                return MHD_YES;

            if (filename == nullptr)
            {
                c.request.form[key].append(data, size);
                return MHD_YES;
            }

            auto &upload = c.request.files[key];
            if (!c.open_files.contains(key))
            {
                upload.path         = ConnectionAccess::nextUploadPath(*c.server);
                upload.file_name    = filename;
                upload.content_type = content_type != nullptr ? content_type : "";

                auto file = ConnectionAccess::fs(*c.server).openForWrite(upload.path, common::WriteMode::Truncate);
                if (!file)
                {
                    c.write_error = file.error().message;
                    return MHD_YES;
                }
                c.open_files[key] = std::move(*file);
            }

            if (size > 0 && c.write_error.empty())
            {
                if (auto written = c.open_files[key]->write(std::as_bytes(std::span(data, size))); !written)
                    c.write_error = written.error().message;
                upload.size += size;
            }
            return MHD_YES;
        }

        MHD_Result sendResponse(MHD_Connection *connection, const Response &r)
        {
            MHD_Response *response = MHD_create_response_from_buffer(r.body.size(), const_cast<char *>(r.body.data()),
                                                                     MHD_RESPMEM_MUST_COPY);
            MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, r.content_type.c_str());
            for (const auto &[name, value] : r.headers)
                MHD_add_response_header(response, name.c_str(), value.c_str());

            MHD_Result result = MHD_queue_response(connection, static_cast<unsigned int>(r.status), response);
            MHD_destroy_response(response);
            return result;
        }

        // called several times per request: first without data, then per piece of the body, finally with size 0
        MHD_Result onRequest(void *cls, MHD_Connection *connection, const char *url, const char *method, const char *,
                             const char *upload_data, size_t *upload_data_size, void **con_cls)
        {
            auto &server = *static_cast<MhdServer *>(cls);
            auto *c      = static_cast<Connection *>(*con_cls);

            if (c == nullptr)
            {
                auto state            = std::make_unique<Connection>();
                state->server         = &server;
                state->request.method = method;
                state->request.path   = url;
                MHD_get_connection_values(connection, MHD_HEADER_KIND, collectValue, &state->request);
                MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND, collectValue, &state->request);

                std::string type = toLower(state->request.header("content-type"));
                state->multipart = type.starts_with("multipart/form-data");
                if (state->multipart || type.starts_with("application/x-www-form-urlencoded"))
                {
                    state->post = MHD_create_post_processor(connection, 32 * 1024, onPostData, state.get());
                    state->bad_form = state->post == nullptr; // e.g. multipart without valid boundary
                }

                *con_cls = state.release(); // deleted in onCompleted
                return MHD_YES;
            }

            if (*upload_data_size != 0)
            {
                const HttpdSettings &s     = ConnectionAccess::settings(server);
                std::size_t          limit = c->multipart ? s.max_multipart_size : s.max_request_size;

                c->received += *upload_data_size;
                // too large or invalid form: the rest is read but ignored, then 413 or 400
                if (c->received > limit)
                    c->too_large = true;
                else if (c->post != nullptr)
                    MHD_post_process(c->post, upload_data, *upload_data_size);
                else if (!c->bad_form)
                    c->request.body.append(upload_data, *upload_data_size);

                *upload_data_size = 0;
                return MHD_YES;
            }

            // request complete; destroying the post processor hands over the data it still holds back
            if (c->post != nullptr)
            {
                MHD_destroy_post_processor(c->post);
                c->post = nullptr;
            }
            for (auto &[name, file] : c->open_files)
            {
                if (auto closed = file->close(); !closed && c->write_error.empty())
                    c->write_error = closed.error().message;
            }
            c->open_files.clear();

            if (c->too_large)
                return sendResponse(connection, Response::json(413, R"({"error":"request too large"})"));
            if (c->bad_form)
                return sendResponse(connection, Response::json(400, R"({"error":"invalid form data"})"));
            if (!c->write_error.empty())
                return sendResponse(connection, Response::json(500, R"({"error":"upload could not be stored"})"));

            return sendResponse(connection, ConnectionAccess::router(server).handle(c->request));
        }

        void onCompleted(void *, MHD_Connection *, void **con_cls, MHD_RequestTerminationCode)
        {
            delete static_cast<Connection *>(*con_cls);
            *con_cls = nullptr;
        }
    }

    MhdServer::MhdServer(const Router &r, common::FileSystem &f) : router(r), fs(f)
    {
    }

    MhdServer::~MhdServer()
    {
        stop();
    }

    std::expected<void, std::string> MhdServer::start(const HttpdSettings &s)
    {
        stop();
        settings = s;
        if (!settings.enabled)
            return {};

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port   = htons(settings.port);
        if (settings.bind_address.empty())
            address.sin_addr.s_addr = htonl(INADDR_ANY);
        else if (inet_pton(AF_INET, settings.bind_address.c_str(), &address.sin_addr) != 1)
            return std::unexpected("invalid bind address: " + settings.bind_address);

        std::vector<MHD_OptionItem> options = {
            {MHD_OPTION_CONNECTION_TIMEOUT, static_cast<intptr_t>(settings.read_timeout.count()), nullptr},
            {MHD_OPTION_NOTIFY_COMPLETED, reinterpret_cast<intptr_t>(&onCompleted), nullptr},
            {MHD_OPTION_SOCK_ADDR, 0, &address},
            {MHD_OPTION_END, 0, nullptr},
        };

        // no MHD_USE_INTERNAL_POLLING_THREAD: libmicrohttpd only works inside MHD_run(), called from poll()
        daemon = MHD_start_daemon(MHD_NO_FLAG, settings.port, nullptr, nullptr, &onRequest, this, MHD_OPTION_ARRAY,
                                  options.data(), MHD_OPTION_END);
        if (daemon == nullptr)
            return std::unexpected("HTTP server could not listen on port " + std::to_string(settings.port));

        return {};
    }

    void MhdServer::stop()
    {
        if (daemon != nullptr)
            MHD_stop_daemon(daemon);
        daemon = nullptr;
    }

    void MhdServer::poll()
    {
        if (daemon != nullptr)
            MHD_run(daemon);
    }

    std::uint16_t MhdServer::port() const
    {
        if (daemon == nullptr)
            return 0;

        const MHD_DaemonInfo *info = MHD_get_daemon_info(daemon, MHD_DAEMON_INFO_BIND_PORT);
        return info != nullptr ? info->port : 0;
    }
}
