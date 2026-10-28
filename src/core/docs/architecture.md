# Architecture of garlic-core

garlic-core replaces the Qt based garlic-lib with the same functionality. The players only contain the GUI, the
multimedia functions and the WebViews they need.

## Target platforms

- Linux, Windows, macOS, Android, iOS (native)
- Browser via WASM (Emscripten): Tizen, LG webOS (only current devices with WASM support), preview in garlic-hub
- ESP32-S3 with 8 MB PSRAM (ESP-IDF 5.x) for E Ink Spectra 6 / eInk displays

## Language and build

- C++23, CMake, no compiler extensions
- No exceptions (`-fno-exceptions`). Errors are returned as `std::expected<T, Error>`. Standard functions that
  terminate on errors (`std::stoi`, `at()` …) are avoided, e.g. `std::from_chars` is used instead.
- Modules are static libraries. Shared types and interfaces (`Error`, `Clock`, `FileSystem` …) live in the module
  `common`. All modules are combined into the shared library `core`.
- Tests live in `tests/<module>`, one test executable per module.
- The HTTP server is an optional target and is not built for WASM. On ESP32 it gets its own backend
  (`esp_http_server`).

## Dependencies

- vcpkg in manifest mode for all native builds (Linux, Windows, macOS, Android, iOS), one CMake preset per platform
- ESP32 and WASM get platform-specific dependencies
- System libraries only for Linux distribution packages
- libcurl with OpenSSL on all native platforms (including macOS/iOS)
- libxml2 (network access and entity substitution disabled), SQLite
- doctest for the unit tests

## Event loop and threads

The core has no event loop of its own and starts no threads. The host (Qt event loop, Android Looper, browser,
ESP32 `loop()`) drives it by calling `poll()`. Qt signals/slots are replaced by callbacks.

Platform specific parts are abstracted by interfaces:

| Interface       | Implementations                                                        |
|-----------------|------------------------------------------------------------------------|
| `Clock`         | system clock, simulated clock for tests                                |
| `Scheduler`     | host specific; exposes the next deadline so the host can (deep) sleep |
| `HttpTransport` | libcurl (`curl_multi`), Emscripten Fetch, Tizen, webOS, ESP32, fake   |
| `FileSystem`    | `std::filesystem`, OPFS/WASMFS, Tizen, webOS, LittleFS/SD, memory      |

## Memory

ESP32-S3 is the reference for memory consumption:

- downloads are streamed in chunks to the `FileSystem`, never buffered completely in RAM
- buffer sizes and queue lengths are configurable
- large data is passed as views (`std::string_view`, `std::span`)
- SMIL and download state can be persisted and restored, because deep sleep loses the RAM

## Downloader

The download queue does not know the HTTP library. It handles order, retries, cache validation (Last-Modified,
Content-Length), redirects and target files, and writes through the `FileSystem` interface. The `HttpTransport`
works step by step, so blocking (ESP32 `esp_http_client`) and non-blocking (`curl_multi`, Fetch) libraries fit.

## Media capabilities

Each player declares the media types it supports at startup (e.g. only `image/bmp` on ESP32). The SMIL parser skips
unsupported media and the downloader does not fetch them. Each skipped medium is written to the event log as
`MEDIA_UNSUPPORTED` with `contentId`, `src` and media type, once per SMIL index version. The play log only contains
what was actually played.

On ESP32, garlic-hub prepares the images (scaling, 6-color dithering) and the player only downloads dithered BMPs.

## SMIL `expr`

`expr` is evaluated with libxml2 (XPath 1.0). The functions missing there are registered as custom XPath
functions: `compare`, `adapi-compare`, `adapi-date`, `adapi-gmdate`, `adapi-weekday`, `adapi-gmweekday`,
`smil-playerId`, `smil-playerName`. The current time comes from the `Clock` interface. `smil-cache` is not
supported; unknown functions make the expression `false` and are logged.
