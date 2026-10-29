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
| `HttpTransport` | libcurl (`curl_multi`), Emscripten Fetch, Tizen, webOS, ESP32, fake   |
| `FileSystem`    | `std::filesystem`, OPFS/WASMFS, Tizen, webOS, LittleFS/SD, memory      |

There is no scheduler interface: every module plans with the `Clock`, and `PlayerRuntimeApi::nextDeadline()` tells
the host when the next `poll()` is due, so it can (deep) sleep until then.

## Memory

ESP32-S3 is the reference for memory consumption:

- downloads are streamed in chunks to the `FileSystem`, never buffered completely in RAM
- buffer sizes and queue lengths are configurable
- large data is passed as views (`std::string_view`, `std::span`)
- planned: SMIL and download state persisted and restored, because deep sleep loses the RAM

## Downloader

The download queue does not know the HTTP library. It handles order, retries, cache validation (Last-Modified,
Content-Length), redirects and target files, and writes through the `FileSystem` interface. The `HttpTransport`
works step by step, so blocking (ESP32 `esp_http_client`) and non-blocking (`curl_multi`, Fetch) libraries fit.

## Media capabilities

Each player declares the media types it supports at startup (`PlayerRuntimeConfig::supports`, e.g. only images on
ESP32). The media manager does not download unsupported media and the engine skips them. Each skipped medium is written to the event log as
`MEDIA_UNSUPPORTED` with `contentId`, `src` and media type, once per SMIL index version. The play log only contains
what was actually played.

On ESP32, garlic-hub prepares the images (scaling, 6-color dithering) and the player only downloads dithered BMPs.

## SMIL `expr`

`expr` is evaluated with libxml2 (XPath 1.0). The functions missing there are registered as custom XPath
functions: `compare`, `adapi-compare`, `adapi-date`, `adapi-gmdate`, `adapi-weekday`, `adapi-gmweekday`,
`smil-playerId`, `smil-playerName`. The current time comes from the `Clock` interface. `smil-cache` is not
supported; unknown functions make the expression `false`, without a log entry.

## Modules

In the order of `CMakeLists.txt`, every one a static library `garlic_core_<module>`, all linked into `libcore`:

| Module           | Does                                                               | Doc                                |
|------------------|--------------------------------------------------------------------|------------------------------------|
| `common`         | clock, file system, MD5, time format                               | [common](common.md)                |
| `logger`         | debug, event, play and task logs                                   | [logger](logger.md)                |
| `downloader`     | HTTP transport, download per file, download queue                  | [downloader](downloader.md)        |
| `database`       | inventory table in SQLite                                          | [database](database.md)            |
| `httpd`          | HTTP server for the REST API                                       | [httpd](httpd.md)                  |
| `configuration`  | `garlic-player.ini`                                                | [configuration](configuration.md)  |
| `rest_api`       | REST API v2                                                        | [rest_api](rest_api.md)            |
| `smil`           | parser, wallclock, `expr`, engine                                  | [smil](smil.md)                    |
| `content`        | index manager, media manager, widgets, cache cleanup               | [content](content.md)              |
| `reports`        | reports to the hub                                                 | [reports](reports.md)              |
| `tasks`          | tasks from the hub                                                 | [tasks](tasks.md)                  |
| `system`         | system information (native only)                                   | [system](system.md)                |
| `player_runtime` | `PlayerRuntimeApi` and `ThreadedRuntime`: the interface for frontends | [player_runtime](player_runtime.md) |

## Decisions

Why the core is built the way it is. The decisions of single modules are in their docs.

- **Same functionality as garlic-lib, without Qt.** One core for all players, also the browser based signage devices
  and the ESP32. The players (QWidget, QML, Java, JS/WASM, ESP32) only do GUI, multimedia and WebViews.
- **Keep the behaviour of garlic-lib.** Playback behaviour that looks odd is not changed on assumption; some of it was
  deliberate (e.g. `pickNumber`). Only clear bugs (crashes, overflows, parsing errors) are fixed without asking, and
  every fix is listed under "Differences to garlic-lib" in the module doc. Where garlic-lib and the SMIL standard
  differ, the doc says which one was chosen.
- **No event loop and no threads of its own, the host calls `poll()`.** In garlic-lib 41 headers hung on `QObject`
  and the SMIL parser ran entirely on `QTimer`s; without Qt someone has to drive the time. A host that polls fits
  blocking libraries (ESP32 `esp_http_client`) and non-blocking ones (`curl_multi`, Fetch). The browser must never
  block, and threads there need extra server headers (COOP/COEP). And the core can be tested without threads and
  without waiting, with a simulated clock. The price: every frontend has to drive the core once.
- **One thread for the whole core, chosen by the host, not the UI thread.** The core blocks briefly (SQLite, writing
  the ini, download chunks, parsing a large index); in the UI thread that makes transitions stutter. Inside, the core
  stays single-threaded, so it needs no locks. `ThreadedRuntime` is the ready-made helper for native frontends, so not
  every frontend writes the same thread code (and its bugs).
- **C++23 with `std::expected`, no exceptions.** Smaller and faster WASM code, and the way to the ESP32 stays open.
  garlic-lib had a single `try`/`catch`, which was not even needed, so this is no break. Consequence: no `std::stoi`
  or `at()` (they terminate), JSON parsed without exceptions and types checked before access. The test programs are
  built with exceptions because doctest's `REQUIRE` needs them; the libraries stay without, so the tested code is the
  product code.
- **ESP32-S3 with 8 MB PSRAM as reference.** The smallest board on which all modules can realistically run; purpose
  is driving E Ink Spectra 6 displays. Hence streaming downloads and configurable buffers. garlic-hub prepares the
  images (scaling, dithering), the ESP32 player only loads dithered BMPs; the core stays free of image processing.
- **WASM only for current devices.** Old Tizen/webOS firmwares without WASM get no extra JS path. No HTTP server in
  the browser: nothing can listen on a port there.
- **libcurl with OpenSSL on every native platform, also macOS/iOS.** One code path instead of an extra URLSession
  transport for Apple. Consequence: Android, iOS, macOS and Windows need a CA bundle shipped with the player.
- **libxml2 for XML, XPath 1.0 with registered functions.** `compare` and the `adapi-*`/`smil-*` functions are real
  XPath functions instead of the string replacement of garlic-lib; no other XPath 2.0 function was used.
- **Dependencies:** vcpkg in manifest mode for native builds, so every platform gets the same versions;
  platform-specific dependencies for ESP32 and WASM; system libraries only for Linux distribution packages. doctest,
  nlohmann/json and miniz also come by FetchContent if they are not found, so the core builds on Linux and macOS
  without preparation.
- **CMake 3.25, not 4.x.** The CMake of the Android SDK and of ESP-IDF is 3.x. Hence the alias for
  `SQLite3::SQLite3` (CMake < 4.3) and `CMAKE_POLICY_VERSION_MINIMUM 3.10` for the fetched miniz and doctest.
- **doctest, no GoogleTest/GoogleMock.** CLion supports doctest, it is leaner, and a later change would be mechanical.
  Instead of mocks there are hand-written fakes (`FakeHttpTransport`, `MemoryFileSystem`, `FakeClock`): tests check the
  end state ("the old file is unchanged, the new one waits as `.ready`") instead of call chains that break with every
  internal change.
- **Flat module directories, namespaces against collisions.** A media player, not a framework: no
  `include/garlic/core/<module>/` trees. Tests in one directory `tests/<module>` that mirrors the modules. `.hpp` for
  C++ headers, so they are told apart from the C headers of curl, libxml2 and SQLite.
- **Modules as static libraries, linked into `libcore`.** Every module can be built and tested alone; `libcore` is
  for the frontends (also Java/Android and JS later). The modules are linked `PRIVATE` with `WHOLE_ARCHIVE`: with
  `PUBLIC` every frontend linked the whole core a second time (2.2 MB instead of 595 KB, and two copies of the core in
  one process). Release builds are stripped (`-s`).
- **No plugin architecture.** A fixed set of modules; the only exchangeable part may later be the SMIL parser, for
  another signage format, and nothing is prepared for it yet.
- **Unsupported media in the event log, not in the play log.** The play log format only knows "played", and it
  stays the list of what really played. Reported once per index version, so an endless playlist does not flood the
  log (flash wear on the ESP32).

## When something goes wrong

- **Where to look:** `debug.log` (texts of the modules), `event_log.xml` (events like `SMIL_OPEN_ERROR`,
  `MEDIA_UNSUPPORTED`, `FAILED_FREEING_SPACE`), `play_log.xml` (what played), `task_execution_log.xml`. All in the log
  directory the frontend gives, the event names are in the module docs.
- **Reproduce in a test, not in the player.** Every module has its own test program (`tests/<module>`), all with
  fakes and the `FakeClock`; a SMIL problem becomes a scenario with a time line (see [smil](smil.md)).
- **CI:** `.github/workflows/ci.yml` builds and tests the core in a `debian:trixie` container (the environment of a
  Debian 13 player) with coverage. The coverage badge is an SVG made by `.github/scripts/coverage_badge.py` and
  committed to the branch `badges`, without an external service. `build.yml` (own runners) starts only after a
  successful CI run on `master` and builds the tested commit (`GARLIC_SHA`).
- **Trixie is older than a developer machine:** it has libxml2 2.9 (hence the version switch in `smil/expr.cpp`),
  CMake 3.31 and Qt 6.8 without the Qt Multimedia private headers.
- **Never compiled yet:** `system/platform_windows.cpp`, `system/platform_apple.cpp` and the Windows branches of
  `StdFileSystem`. Errors there show up only with the first Windows or Apple build.
