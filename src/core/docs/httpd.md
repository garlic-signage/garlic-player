# httpd

HTTP server for the REST API of the player. Namespace `garlic::core::httpd`.

```
Router + Request/Response     plain C++, what the controllers see
        │
Server (interface)            start / stop / poll / port
        │
MhdServer                     libmicrohttpd, the only file that knows the library
```

The library can be replaced by another `Server` implementation (planned: `esp_http_server` for ESP32) without
touching the controllers.

## Usage

```cpp
Router router;
router.add("POST", "/v2/oauth2/token", [&](const Request &r) { return Response::json(200, token(r)); });
router.add("GET",  "/v2/files/{id}",   [&](const Request &r) { return fileInfo(r.path_params.at("id")); });

MhdServer server{router, fs};
if (auto started = server.start(config.httpd()); !started)
    log(started.error());
// core thread: server.poll(); together with transport.poll(), queue.poll(), …
```

- **No threads.** libmicrohttpd runs without its own thread; `poll()` calls `MHD_run()` and returns immediately.
  The handlers run in the core thread and can use configuration, database and file system directly.
- **Switching off:** at build time with `GARLIC_CORE_WITH_HTTPD=OFF` (default off for WASM/ESP32), at runtime with
  `httpd/enabled=false` in the ini; `start()` then does nothing.

## Request

| Field          | Content                                                              |
|----------------|----------------------------------------------------------------------|
| `method`, `path` | e.g. `POST`, `/v2/files/new` (without query)                       |
| `query`        | URL parameters                                                       |
| `form`         | fields of `application/x-www-form-urlencoded` and `multipart/form-data` |
| `files`        | file fields of multipart: temporary file, name, content type, size   |
| `headers`      | names in lowercase; `header(name)` is case-insensitive               |
| `path_params`  | placeholders of the route, e.g. `{id}`                               |
| `body`         | raw body if it is no form, e.g. JSON                                 |

`param(name)` returns the URL parameter or else the form field, like `getParameter()` of QtWebApp.

**Uploads** are written in pieces through the `FileSystem` into `upload_dir` (the cache directory) as temporary files,
never held in RAM. They are removed after the handler; a handler keeps a file by renaming it.

## Router

Routes are added in order, the first match wins. Placeholders like `{id}` match one path segment. Unknown path: 404,
known path with another method: 405.

## Answers that come later

A handler that cannot answer at once (e.g. a screenshot made by the frontend) returns `Response::later(pending)` and
keeps the `std::shared_ptr<PendingResponse>`. The server holds the connection (`MHD_suspend_connection`, the daemon
is started with `MHD_ALLOW_SUSPEND_RESUME`) and goes on serving other requests. When `pending->complete(response)` has
been called, the next `poll()` resumes the connection and sends the answer. Held connections still open at `stop()`
get 503.

```cpp
router.add("GET", "/slow", [&](const Request &) { pending = std::make_shared<PendingResponse>(); return Response::later(pending); });
…
pending->complete(Response{200, "image/jpeg", jpeg, {}});   // later, from the core thread
```

## Limits and errors

| Case                                         | Response |
|----------------------------------------------|----------|
| body larger than `max_request_size` (no multipart) or `max_multipart_size` (multipart) | 413 |
| form that cannot be parsed (e.g. multipart without boundary) | 400 |
| upload could not be stored                    | 500      |

## Settings

`HttpdSettings` (`httpd/httpd_settings.hpp`), filled by `Configuration::httpd()` from the group `[httpd]`:

| ini key            | Field                | Default     |
|--------------------|----------------------|-------------|
| `enabled`          | `enabled`            | `true`      |
| `host`             | `bind_address`       | all IPv4 interfaces |
| `port`             | `port`               | 8080        |
| `readTimeout` (ms) | `read_timeout` (s)   | 60          |
| `maxRequestSize`   | `max_request_size`   | 65536       |
| `maxMultiPartSize` | `max_multipart_size` | 114857600   |
| —                  | `upload_dir`         | cache directory |

Defaults apply only for missing or invalid keys and are not written into the ini. The other keys of garlic-lib
(`minThreads`, `maxThreads`, `cleanupInterval`, `cacheSize`, …) belong to QtWebApp and are ignored.

## Differences to garlic-lib

- garlic-lib wrote the defaults into the ini on every start, so a changed port was overwritten. Now the ini wins.
- `/cache/…` (static files from the cache) is not served; it was not used (the calls in the players were commented
  out).
- IPv4 only for now.
