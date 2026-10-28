# Downloader

The module `downloader` loads media and SMIL indexes into the cache directory. It consists of three layers:

```
DownloadQueue      jobs, order, parallel downloads, result callbacks
     │
Downloader         logic per file: cache validation, redirects, content type, disk space, inventory, writing
     │
HttpTransport      executes one HTTP request → status, headers, data in chunks
```

Only `HttpTransport` is platform specific. `Downloader` and `DownloadQueue` are plain C++ and are tested with a fake
transport.

## HttpTransport

Header: `downloader/http_transport.hpp`

```cpp
auto id = transport.start({HttpMethod::Head, url, headers}, handler); // never calls the handler directly
transport.poll();   // the host calls this regularly; callbacks only happen in here
transport.cancel(*id);
```

The response arrives at an `HttpResponseHandler`:

| Callback       | When                                                                     |
|----------------|--------------------------------------------------------------------------|
| `onHead()`     | once: status code and headers                                            |
| `onData()`     | per data chunk, never for HEAD; return `false` aborts the request        |
| `onComplete()` | always last: success or `TransportError`                                 |

Rules for every implementation:

- **No callbacks from `start()`**, only from `poll()`. A handler can therefore start the next request (HEAD → GET)
  from within a callback without reentrancy problems.
- **No visible threads.** A blocking library (ESP32 `esp_http_client`) does a small step per `poll()`, a
  non-blocking one (`curl_multi`, Fetch) processes whatever has arrived.
- **Redirects are not followed**; the `Downloader` handles 3xx itself. Exception: the browser follows redirects itself
  with Fetch, then the final response arrives.
- **HTTP status codes are not errors.** 404 or 500 arrive in `onHead()`. `TransportError` only means that no HTTP
  response could be received (DNS, connection, TLS, timeout).
- **Chunks are not buffered.** The data passed to `onData()` is only valid during the call.
- **After `cancel()` no callbacks** for that request arrive anymore.
- **Methods**: `Head`, `Get`, and `Put` with `HttpRequest::body` for uploads (the reports to the hub).

Timeouts for stalled downloads are not part of the transport. The `Downloader` handles them platform-independent via
the `Clock` interface.

### Implementations

| Implementation      | Platform                                | Status   |
|---------------------|-----------------------------------------|----------|
| `FakeHttpTransport` | tests (`tests/downloader`)              | done     |
| `CurlTransport`     | Linux, Windows, macOS, Android, iOS     | done     |
| `FetchTransport`    | browser (WASM)                          | planned  |
| Tizen, webOS        | via JS bridge                           | planned  |
| ESP32               | `esp_http_client`                       | planned  |

## Downloader

Header: `downloader/downloader.hpp`

```cpp
Downloader downloader{transport, fs, inventory, clock, config};
downloader.start(url, "", [](const DownloadResult &r) { play(r.local_path); });
// host loop: transport.poll(); downloader.poll();
```

One job at a time. `on_finished` is called exactly once, always from inside a `poll()`.

### Flow

1. HEAD, conditional (`If-None-Match`, `If-Modified-Since`) if the cached file exists
2. 3xx: follow `Location` (absolute or relative), at most `max_redirects`
3. 304, or the same ETag (or Last-Modified + size without ETag): `NotModified`
4. `text/html`: `NotCacheable`; unknown content type: `Failed` (except `.wgt` and `.smil`). These checks are for
   media: a file with a fixed name (index, task list, configuration, firmware) is a system file and taken with any
   content type, e.g. a firmware as `application/octet-stream`.
5. `make_space(bytes)`, inventory state `Transfer`
6. GET into `<name>.part`; resume with `Range` + `If-Range` if the server still has the same version
7. check: not empty, size matches `Content-Length`
8. rename to `<name>`, or to `<name>.ready` if `<name>` exists; inventory state `Complete`

### Results

| Status         | `local_path`                                   |
|----------------|------------------------------------------------|
| `Downloaded`   | `<name>`; the new file may wait as `<name>.ready` |
| `NotModified`  | `<name>`                                       |
| `NotCacheable` | empty, the player uses the URL                 |
| `Failed`       | the cached copy if there is one, else empty    |

**The player keeps playing on errors.** No error deletes or overwrites the cached file: HTTP errors, connection
loss, timeout, wrong size, no disk space. `Failed` returns the cached copy in `local_path`; `error` and `http_status`
are for logging.

Without ETag and Last-Modified the downloader cannot tell whether a file changed, so it downloads it again (the new
file waits as `.ready`). This is the safe choice: a changed file with the same size is never missed.

### File names

`MD5(original URL) + "." + extension`. The extension comes from the URL; from the content type if the URL has none,
is a script (`.php`, `.asp`, …) or the server sends `application/octet-stream`. A fixed name (e.g. `index.smil`) can
be passed to `start()`.

### Inventory

`Inventory` (`downloader/inventory.hpp`) stores per URL: file name, ETag and Last-Modified exactly as sent by the
server, content type, size and state. Implemented by the database module later, `MemoryInventory` in the tests.

### Differences to garlic-lib

- ETag and Last-Modified are sent back unchanged (garlic-lib removed the quotes of the ETag and used its own
  download time as If-Modified-Since).
- Several redirects instead of one; `Location` may be relative.
- Resume checks with `If-Range` that the file did not change.
- Idle timeout for stalled downloads.
- Not ported: computing the MD5 of the local file as ETag when the server changes (`handleDifferentServer`).

## DownloadQueue

Header: `downloader/download_queue.hpp`

```cpp
DownloadQueue queue{transport, fs, inventory, clock, config, 4, [](const DownloadResult &r) { … }};
queue.add(index_url, "index.smil");
queue.add(media_url);
// host loop: transport.poll(); queue.poll();
```

- `max_parallel` Downloaders are created once and reused (0 counts as 1).
- Jobs run in the order they were added.
- A URL that is waiting or running is ignored when added again; after it finished it can be added again.
- One callback for all results, exactly once per job, always from inside a `poll()`. Jobs can be added from inside
  the callback (e.g. after the index was loaded).
- `clear()` drops waiting jobs and cancels running ones without callbacks.
- The queue does not call `HttpTransport::poll()`; the host does, so other modules can share the transport.

## CurlTransport

Header: `downloader/curl_transport.hpp`, only built with `GARLIC_CORE_WITH_CURL` (default on, off for WASM/ESP32).

```cpp
CurlTransport transport;                       // or CurlTransport{{.ca_bundle = "/path/cacert.pem"}}
```

- `curl_multi` without threads: each `poll()` lets libcurl do what is due and returns immediately.
- Redirects are not followed (`CURLOPT_FOLLOWLOCATION` off), compression is off (`Accept-Encoding` not sent), so
  `Content-Length` matches the bytes written.
- Proxies from the environment (`http_proxy`, `https_proxy`) are used by libcurl automatically.
- `start()` and `cancel()` may be called from inside handler callbacks; libcurl forbids changing its handles there,
  so these changes are applied in the next `poll()`.
- **CA certificates:** on Linux OpenSSL finds the system certificates. Android, iOS, macOS and Windows have no
  OpenSSL certificate store; there `ca_bundle` must point to a PEM file (e.g. the Mozilla bundle shipped with the
  player).

Tests (`tests/downloader/test_curl_transport.cpp`) run against a small HTTP server on `127.0.0.1` inside the test
(POSIX only), including a complete download through the `Downloader` with a redirect. No internet is needed.
