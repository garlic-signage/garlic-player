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
#include "downloader/curl_transport.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <charconv>

namespace garlic::core::downloader
{
    struct CurlTransport::Transfer
    {
        CURL                *easy    = nullptr;
        curl_slist          *headers = nullptr;
        HttpResponseHandler *handler = nullptr;
        HttpResponseHead     head;
        bool                 head_sent         = false;
        bool                 aborted_by_handler = false;
        bool                 cancelled         = false;
        bool                 added             = false; // added to the multi handle
        char                 error[CURL_ERROR_SIZE] = {};

        ~Transfer()
        {
            curl_slist_free_all(headers);
            if (easy != nullptr)
                curl_easy_cleanup(easy);
        }
    };

    namespace
    {
        void globalInit()
        {
            // once per process; never cleaned up, the library stays in use until the process ends
            static const bool initialized = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
            (void)initialized;
        }

        std::string_view trim(std::string_view text)
        {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
                text.remove_prefix(1);
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r' ||
                                     text.back() == '\n'))
                text.remove_suffix(1);
            return text;
        }

        /// "HTTP/1.1 200 OK", "HTTP/2 304" → status code, 0 if it is no status line
        int parseStatusLine(std::string_view line)
        {
            if (!line.starts_with("HTTP/"))
                return 0;

            auto space = line.find(' ');
            if (space == std::string_view::npos)
                return 0;

            std::string_view code   = line.substr(space + 1, 3);
            int              status = 0;
            std::from_chars(code.data(), code.data() + code.size(), status);
            return status;
        }

        TransportErrorCode toErrorCode(CURLcode code)
        {
            switch (code)
            {
                case CURLE_UNSUPPORTED_PROTOCOL:
                case CURLE_URL_MALFORMAT:
                    return TransportErrorCode::InvalidRequest;
                case CURLE_COULDNT_RESOLVE_PROXY:
                case CURLE_COULDNT_RESOLVE_HOST:
                case CURLE_COULDNT_CONNECT:
                case CURLE_SEND_ERROR:
                case CURLE_RECV_ERROR:
                case CURLE_GOT_NOTHING:
                case CURLE_PARTIAL_FILE:
                    return TransportErrorCode::ConnectionFailed;
                case CURLE_SSL_CONNECT_ERROR:
                case CURLE_PEER_FAILED_VERIFICATION:
                case CURLE_SSL_CERTPROBLEM:
                case CURLE_SSL_CIPHER:
                case CURLE_SSL_CACERT_BADFILE:
                    return TransportErrorCode::TlsFailed;
                case CURLE_OPERATION_TIMEDOUT:
                    return TransportErrorCode::Timeout;
                default:
                    return TransportErrorCode::Other;
            }
        }

        void sendHead(CurlTransport::Transfer &t)
        {
            if (t.head_sent)
                return;
            t.head_sent = true;
            t.handler->onHead(t.head);
        }

        // Called by libcurl for every header line, including the status line and the empty line at the end.
        std::size_t onHeaderLine(char *data, std::size_t size, std::size_t count, void *user)
        {
            auto            &t    = *static_cast<CurlTransport::Transfer *>(user);
            std::size_t      len  = size * count;
            std::string_view line = trim(std::string_view(data, len));

            if (t.cancelled)
                return 0;

            if (int status = parseStatusLine(line); status != 0)
            {
                // a new response starts (e.g. after "100 Continue"): forget the previous headers
                t.head = {status, {}};
                return len;
            }

            if (line.empty())
            {
                // end of the headers; interim 1xx responses are not passed on
                if (t.head.status >= 200)
                    sendHead(t);
                return len;
            }

            auto colon = line.find(':');
            if (colon != std::string_view::npos)
                t.head.headers.add(std::string(trim(line.substr(0, colon))), std::string(trim(line.substr(colon + 1))));

            return len;
        }

        std::size_t onBody(char *data, std::size_t size, std::size_t count, void *user)
        {
            auto       &t   = *static_cast<CurlTransport::Transfer *>(user);
            std::size_t len = size * count;

            if (t.cancelled)
                return 0;

            sendHead(t);
            if (!t.handler->onData(std::as_bytes(std::span(data, len))))
            {
                t.aborted_by_handler = true;
                return 0; // makes libcurl abort with CURLE_WRITE_ERROR
            }
            return len;
        }
    }

    CurlTransport::CurlTransport(CurlTransportConfig cfg) : config(std::move(cfg))
    {
        globalInit();
        multi = curl_multi_init();
    }

    CurlTransport::~CurlTransport()
    {
        for (auto it = transfers.begin(); it != transfers.end();)
            remove(it++);
        curl_multi_cleanup(static_cast<CURLM *>(multi));
    }

    std::expected<RequestId, TransportError> CurlTransport::start(const HttpRequest &request,
                                                                  HttpResponseHandler &handler)
    {
        auto t     = std::make_unique<Transfer>();
        t->handler = &handler;
        t->easy    = curl_easy_init();
        if (t->easy == nullptr)
            return std::unexpected(TransportError{TransportErrorCode::Other, "curl_easy_init failed"});

        for (const auto &header : request.headers.all())
            t->headers = curl_slist_append(t->headers, (header.name + ": " + header.value).c_str());

        CURL *easy = t->easy;
        curl_easy_setopt(easy, CURLOPT_URL, request.url.c_str());
        if (request.method == HttpMethod::Head)
            curl_easy_setopt(easy, CURLOPT_NOBODY, 1L);
        else
            curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(easy, CURLOPT_HTTPHEADER, t->headers);
        curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 0L); // the Downloader handles redirects
        curl_easy_setopt(easy, CURLOPT_SUPPRESS_CONNECT_HEADERS, 1L);
        curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, static_cast<long>(config.connect_timeout.count()));
        curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, onHeaderLine);
        curl_easy_setopt(easy, CURLOPT_HEADERDATA, t.get());
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, onBody);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, t.get());
        curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, t->error);
        curl_easy_setopt(easy, CURLOPT_PRIVATE, t.get());
        if (!config.ca_bundle.empty())
            curl_easy_setopt(easy, CURLOPT_CAINFO, config.ca_bundle.c_str());

        // libcurl forbids adding handles from inside its callbacks; then poll() adds it
        if (!in_perform)
        {
            if (curl_multi_add_handle(static_cast<CURLM *>(multi), easy) != CURLM_OK)
                return std::unexpected(TransportError{TransportErrorCode::Other, "curl_multi_add_handle failed"});
            t->added = true;
        }

        RequestId id = next_id++;
        transfers.emplace(id, std::move(t));
        return id;
    }

    void CurlTransport::cancel(RequestId id)
    {
        auto it = transfers.find(id);
        if (it == transfers.end())
            return;

        // inside a libcurl callback the handle must not be removed: mark it, poll() cleans up
        if (in_perform)
            it->second->cancelled = true;
        else
            remove(it);
    }

    void CurlTransport::poll()
    {
        auto *m = static_cast<CURLM *>(multi);

        for (auto &[id, t] : transfers)
        {
            if (!t->added && !t->cancelled)
            {
                curl_multi_add_handle(m, t->easy);
                t->added = true;
            }
        }

        int running = 0;
        in_perform  = true;
        curl_multi_perform(m, &running);
        in_perform = false;

        // finished transfers
        int      queued = 0;
        CURLMsg *msg    = nullptr;
        while ((msg = curl_multi_info_read(m, &queued)) != nullptr)
        {
            if (msg->msg != CURLMSG_DONE)
                continue;

            CURLcode  code = msg->data.result;
            Transfer *done = nullptr;
            curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, reinterpret_cast<char **>(&done));

            auto it = std::find_if(transfers.begin(), transfers.end(),
                                   [done](const auto &entry) { return entry.second.get() == done; });
            if (it == transfers.end() || it->second->cancelled)
                continue;

            // keep the transfer alive for the callbacks, but out of the map: the handler may start new requests
            std::unique_ptr<Transfer> t = std::move(it->second);
            curl_multi_remove_handle(m, t->easy);
            t->added = false;
            transfers.erase(it);

            if (code == CURLE_OK)
            {
                sendHead(*t);
                t->handler->onComplete({});
            }
            else
            {
                TransportErrorCode error = t->aborted_by_handler ? TransportErrorCode::Cancelled : toErrorCode(code);
                std::string message      = t->error[0] != '\0' ? std::string(t->error) : curl_easy_strerror(code);
                t->handler->onComplete(std::unexpected(TransportError{error, message}));
            }
        }

        // requests cancelled during the callbacks
        for (auto it = transfers.begin(); it != transfers.end();)
        {
            if (it->second->cancelled)
                remove(it++);
            else
                ++it;
        }
    }

    void CurlTransport::remove(std::map<RequestId, std::unique_ptr<Transfer>>::iterator it)
    {
        if (it->second->added)
            curl_multi_remove_handle(static_cast<CURLM *>(multi), it->second->easy);
        transfers.erase(it);
    }
}
