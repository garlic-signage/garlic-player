# garlic-core

Qt-free rewrite of the garlic-lib in C++23. It provides the player-independent functions (download, database,
HTTP server, SMIL parser, configuration, logger, system information) to the players (QWidget, QML, Java,
JavaScript/WASM, ESP32).

See [docs/architecture.md](docs/architecture.md) for the design decisions. Modules: [common](docs/common.md),
[logger](docs/logger.md), [downloader](docs/downloader.md), [database](docs/database.md), [httpd](docs/httpd.md),
[configuration](docs/configuration.md), [rest_api](docs/rest_api.md), [smil](docs/smil.md).

## Layout

```
src/core/
├── CMakeLists.txt        options, modules, tests, shared library 'core'
├── CMakePresets.json     build presets per platform
├── vcpkg.json            dependencies (vcpkg manifest mode)
├── cmake/                CMake helpers
├── docs/                 documentation
├── common/               shared types and interfaces (.hpp and .cpp side by side)
├── logger/
├── downloader/
├── database/
├── httpd/
├── configuration/
├── rest_api/
├── smil/                SMIL parser
└── tests/                one test executable per module
    ├── common/
    ├── logger/
    ├── downloader/
    ├── database/
    ├── httpd/
    ├── configuration/
    ├── rest_api/
    ├── smil/
    ├── data/             test files (SMIL)
    └── support/          fakes shared by the tests
```

Every module is a flat directory and a static library `garlic_core_<module>` in the namespace
`garlic::core::<module>`. Includes are relative to `src/core`, e.g. `#include "downloader/http_transport.hpp"`.
At the end all modules are linked into the shared library `core`. On WASM and ESP32 no shared library is built;
the static modules are linked directly.

## Build

Requirements: CMake ≥ 3.25, Ninja, a C++23 compiler (GCC ≥ 12, Clang ≥ 16 with libc++, MSVC ≥ 17.3) and vcpkg with
`VCPKG_ROOT` set.

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

### Tests

The tests use [doctest](https://github.com/doctest/doctest). It is taken from vcpkg or the system if available,
otherwise CMake downloads it (FetchContent). So the tests also run without vcpkg:

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

The libraries are built without exceptions, the test executables with exceptions (doctest needs them for `REQUIRE`).

### Options

| Option                    | Default                     | Meaning                                       |
|---------------------------|-----------------------------|-----------------------------------------------|
| `GARLIC_CORE_BUILD_SHARED`| `ON` (`OFF` on WASM/ESP32)  | build the shared library `core`               |
| `GARLIC_CORE_WITH_CURL`   | `ON` (`OFF` on WASM/ESP32)  | build the CurlTransport (needs libcurl)       |
| `GARLIC_CORE_WITH_HTTPD`  | `ON` (`OFF` on WASM/ESP32)  | build the HTTP server (needs libmicrohttpd)   |
| `GARLIC_CORE_BUILD_TESTS` | `ON` if top-level project   | build the unit tests                          |
