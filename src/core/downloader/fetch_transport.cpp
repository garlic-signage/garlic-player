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
#include "downloader/fetch_transport.hpp"

#include <emscripten/em_js.h>

#include <string>
#include <string_view>

// The JS side. One entry per request in Module['garlicFetch'].requests; fetch() and the stream reader fill it in the
// event loop of the browser, the C++ side only asks for what has arrived. So no JS calls back into C++.
//
// Read-ahead: the reader runs until 1 MiB is waiting, then it pauses until poll() has taken data. So the speed does
// not depend on how often poll() is called, and a video is never held completely in memory.

EM_JS_DEPS(garlic_fetch, "$UTF8ToString,$lengthBytesUTF8,$stringToUTF8");

EM_JS(void, garlic_fetch_init, (), {
    if (Module["garlicFetch"])
        return;

    const pump = (r) => {
        if (r.reading || r.done || r.error !== null || r.queued >= (1 << 20))
            return;
        r.reading = true;
        r.reader.read().then(
            (chunk) => {
                r.reading = false;
                if (chunk.done)
                    r.done = true;
                else
                {
                    r.queue.push(chunk.value);
                    r.queued += chunk.value.length;
                    pump(r);
                }
            },
            (e) => {
                r.reading = false;
                r.error   = String(e);
            });
    };

    Module["garlicFetch"] = {requests : new Map(), pump : pump};
});

// headers: "name\nvalue\n" for each header. Returns false if the URL or the request is invalid.
EM_JS(bool, garlic_fetch_start,
      (int id, const char *method, const char *url, const char *headers, const char *body, int body_len), {
          const g   = Module["garlicFetch"];
          const req = {
              controller : new AbortController(),
              status : 0,
              headers : "",
              reader : null,
              queue : [],
              queued : 0,
              reading : false,
              done : false,
              error : null
          };

          const init = {
              method : UTF8ToString(method),
              headers : new Headers(),
              signal : req.controller.signal,
              redirect : "follow", // fetch cannot stop at a 3xx, the browser follows it
              cache : "no-store"   // the Downloader validates the cache itself, like on the other platforms
          };
          try
          {
              new URL(UTF8ToString(url));
              const lines = UTF8ToString(headers).split("\n");
              for (let i = 0; i + 1 < lines.length; i += 2)
                  init.headers.append(lines[i], lines[i + 1]);
          }
          catch (e)
          {
              return false;
          }
          if (init.method === "PUT")
              init.body = HEAPU8.slice(body, body + body_len);

          g.requests.set(id, req);
          fetch(UTF8ToString(url), init)
              .then(
                  (response) => {
                      let text = "";
                      response.headers.forEach((value, name) => { text += name + ": " + value + "\n"; });
                      req.headers = text;
                      req.status  = response.status;
                      if (response.body === null)
                          req.done = true; // HEAD, 204, 304
                      else
                      {
                          req.reader = response.body.getReader();
                          g.pump(req);
                      }
                  },
                  (e) => {
                      req.error  = String(e);
                      req.status = -1;
                  });
          return true;
      });

// 0: no response yet, -1: failed (error text: garlic_fetch_text), otherwise the HTTP status
EM_JS(int, garlic_fetch_status, (int id), {
    const req = Module["garlicFetch"].requests.get(id);
    return req ? req.status : -1;
});

// which 0: the response headers ("name: value\n" for each header), 1: the error text. Copies them into target if it
// is large enough; returns the bytes needed including the terminating 0.
EM_JS(int, garlic_fetch_text, (int id, int which, char *target, int capacity), {
    const req  = Module["garlicFetch"].requests.get(id);
    const text = !req ? "request unknown" : which === 0 ? req.headers : req.error !== null ? req.error : "";
    const size = lengthBytesUTF8(text) + 1;
    if (size <= capacity)
        stringToUTF8(text, target, capacity);
    return size;
});

// Copies up to capacity bytes of waiting data. Returns the count, 0 if nothing is waiting, -1 at the end of the body,
// -2 on an error (error text: garlic_fetch_text).
EM_JS(int, garlic_fetch_read, (int id, std::byte *target, int capacity), {
    const g   = Module["garlicFetch"];
    const req = g.requests.get(id);
    if (!req)
        return -2;

    if (req.queue.length > 0)
    {
        const chunk = req.queue[0];
        const count = Math.min(chunk.length, capacity);
        HEAPU8.set(chunk.subarray(0, count), target);
        if (count < chunk.length)
            req.queue[0] = chunk.subarray(count);
        else
            req.queue.shift();
        req.queued -= count;
        g.pump(req);
        return count;
    }
    if (req.error !== null)
        return -2;
    return req.done ? -1 : 0;
});

// Aborts a running request (if any) and forgets it
EM_JS(void, garlic_fetch_forget, (int id), {
    const g   = Module["garlicFetch"];
    const req = g.requests.get(id);
    if (!req)
        return;
    g.requests.delete(id);
    if (!req.done && req.error === null)
        req.controller.abort();
});

namespace garlic::core::downloader
{
    namespace
    {
        constexpr std::size_t chunk_size = 64 * 1024;

        int jsId(RequestId id)
        {
            return static_cast<int>(id);
        }

        /// which 0: response headers, 1: error text
        std::string jsText(RequestId id, int which)
        {
            std::string text(static_cast<std::size_t>(garlic_fetch_text(jsId(id), which, nullptr, 0)), '\0');
            garlic_fetch_text(jsId(id), which, text.data(), static_cast<int>(text.size()));
            text.pop_back(); // the terminating 0
            return text;
        }

        std::string_view trim(std::string_view text)
        {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
                text.remove_prefix(1);
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'))
                text.remove_suffix(1);
            return text;
        }

        HttpHeaders parseHeaders(std::string_view text)
        {
            HttpHeaders headers;
            while (!text.empty())
            {
                auto             end  = text.find('\n');
                std::string_view line = text.substr(0, end);
                text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);

                auto colon = line.find(':');
                if (colon != std::string_view::npos)
                    headers.add(std::string(trim(line.substr(0, colon))), std::string(trim(line.substr(colon + 1))));
            }
            return headers;
        }

        bool equalsIgnoreCase(std::string_view a, std::string_view b)
        {
            if (a.size() != b.size())
                return false;
            for (std::size_t i = 0; i < a.size(); ++i)
            {
                auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
                if (lower(a[i]) != lower(b[i]))
                    return false;
            }
            return true;
        }

        const char *methodName(HttpMethod method)
        {
            switch (method)
            {
                case HttpMethod::Head:
                    return "HEAD";
                case HttpMethod::Put:
                    return "PUT";
                case HttpMethod::Get:
                    break;
            }
            return "GET";
        }
    }

    FetchTransport::FetchTransport() : buffer(chunk_size)
    {
        garlic_fetch_init();
    }

    FetchTransport::~FetchTransport()
    {
        for (const auto &[id, transfer] : transfers)
            garlic_fetch_forget(jsId(id));
    }

    std::expected<RequestId, TransportError> FetchTransport::start(const HttpRequest &request,
                                                                   HttpResponseHandler &handler)
    {
        // The browser does not let a script set User-Agent, it always sends its own. The hub reads the one of the
        // player from X-Garlic-User-Agent instead.
        std::string headers;
        for (const auto &header : request.headers.all())
        {
            headers += equalsIgnoreCase(header.name, "User-Agent") ? "X-Garlic-User-Agent" : header.name;
            headers += '\n';
            headers += header.value;
            headers += '\n';
        }

        RequestId id = next_id++;
        if (!garlic_fetch_start(jsId(id), methodName(request.method), request.url.c_str(), headers.c_str(),
                                request.body.data(), static_cast<int>(request.body.size())))
            return std::unexpected(TransportError{TransportErrorCode::InvalidRequest, "invalid request: " + request.url});

        transfers.emplace(id, Transfer{&handler});
        return id;
    }

    void FetchTransport::cancel(RequestId id)
    {
        if (transfers.erase(id) > 0)
            garlic_fetch_forget(jsId(id));
    }

    void FetchTransport::poll()
    {
        // the handlers may start or cancel requests during the callbacks, so go over a copy of the ids
        std::vector<RequestId> ids;
        ids.reserve(transfers.size());
        for (const auto &[id, transfer] : transfers)
            ids.push_back(id);

        for (RequestId id : ids)
            pollOne(id);
    }

    void FetchTransport::pollOne(RequestId id)
    {
        auto it = transfers.find(id);
        if (it == transfers.end())
            return;

        if (!it->second.head_sent)
        {
            int status = garlic_fetch_status(jsId(id));
            if (status == 0)
                return;
            if (status < 0)
            {
                finish(id, std::unexpected(TransportError{TransportErrorCode::ConnectionFailed, jsText(id, 1)}));
                return;
            }

            HttpResponseHead head{status, parseHeaders(jsText(id, 0))};
            it->second.head_sent = true;
            it->second.handler->onHead(head);
        }

        // everything that is waiting; the read-ahead in JS limits it
        while (true)
        {
            it = transfers.find(id); // the handler may have cancelled it
            if (it == transfers.end())
                return;

            int count = garlic_fetch_read(jsId(id), buffer.data(), static_cast<int>(buffer.size()));
            if (count == 0)
                return;
            if (count == -1)
            {
                finish(id, {});
                return;
            }
            if (count < 0)
            {
                finish(id, std::unexpected(TransportError{TransportErrorCode::ConnectionFailed, jsText(id, 1)}));
                return;
            }

            if (!it->second.handler->onData(std::span(buffer.data(), static_cast<std::size_t>(count))))
            {
                finish(id, std::unexpected(TransportError{TransportErrorCode::Cancelled, "aborted by the handler"}));
                return;
            }
        }
    }

    void FetchTransport::finish(RequestId id, std::expected<void, TransportError> result)
    {
        // out of the map before the callback: the handler may start new requests
        auto                 it      = transfers.find(id);
        HttpResponseHandler *handler = it->second.handler;
        transfers.erase(it);
        garlic_fetch_forget(jsId(id));
        handler->onComplete(std::move(result));
    }
}
