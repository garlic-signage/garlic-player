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

Timeouts for stalled downloads are not part of the transport. The `Downloader` handles them platform-independent via
the `Clock` interface.

### Implementations

| Implementation      | Platform                                | Status   |
|---------------------|-----------------------------------------|----------|
| `FakeHttpTransport` | tests (`tests/downloader`)              | done     |
| `CurlTransport`     | Linux, Windows, macOS, Android, iOS     | planned  |
| `FetchTransport`    | browser (WASM)                          | planned  |
| Tizen, webOS        | via JS bridge                           | planned  |
| ESP32               | `esp_http_client`                       | planned  |
