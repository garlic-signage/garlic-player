# garlic-core

Qt-free rewrite of the garlic-lib in C++23. It provides the player-independent functions (download, database,
HTTP server, SMIL parser, configuration, logger, system information) to the players (QWidget, QML, Java,
JavaScript/WASM, ESP32).

See [docs/architecture.md](docs/architecture.md) for the design decisions. Modules: [common](docs/common.md),
[downloader](docs/downloader.md).

## Layout

```
src/core/
├── CMakeLists.txt        top level: options, modules, tests, shared library 'core'
├── CMakePresets.json     build presets per platform
├── vcpkg.json            dependencies (vcpkg manifest mode)
├── cmake/                common CMake helpers
├── docs/                 documentation
├── common/               static module: shared types, interfaces, version
│   ├── include/garlic/core/common/
│   └── src/
├── downloader/           static module
│   ├── include/garlic/core/downloader/
│   └── src/
└── tests/                one test executable per module
    ├── common/
    └── downloader/
```

Every module is a static library `garlic_core_<module>` (alias `garlic::core::<module>`, namespace
`garlic::core::<module>`). The tests mirror the module structure under `tests/<module>`; each module has its own test
executable, so it can be built and tested separately. At the end all modules are linked into the shared library `core`
(`libcore.so`, `core.dll`, `libcore.dylib`). On WASM and ESP32 no shared library is built; the static modules are
linked directly.

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
| `GARLIC_CORE_BUILD_TESTS` | `ON` if top-level project   | build the unit tests                          |
