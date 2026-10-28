cmake_minimum_required(VERSION 3.25)

project(garlic-core
    VERSION 0.1.0
    DESCRIPTION "Qt-free core library of the garlic-player"
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GarlicCoreOptions)

# WASM and ESP32: no shared library (the static modules are linked directly), no libcurl, no libmicrohttpd
if(EMSCRIPTEN OR ESP_PLATFORM)
    set(_garlic_core_native OFF)
else()
    set(_garlic_core_native ON)
endif()

option(GARLIC_CORE_BUILD_SHARED "Build the shared library 'core' from all static modules" ${_garlic_core_native})
option(GARLIC_CORE_WITH_CURL "Build the CurlTransport (libcurl)" ${_garlic_core_native})
option(GARLIC_CORE_WITH_HTTPD "Build the HTTP server (libmicrohttpd)" ${_garlic_core_native})
option(GARLIC_CORE_BUILD_TESTS "Build the unit tests of the modules" ${PROJECT_IS_TOP_LEVEL})

# Static modules are linked into a shared library later, so they need PIC.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# nlohmann/json (header-only): from vcpkg or the system, otherwise downloaded
include(FetchContent)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY    https://github.com/nlohmann/json.git
    GIT_TAG           v3.11.3
    GIT_SHALLOW       TRUE
    SYSTEM
    FIND_PACKAGE_ARGS CONFIG
)
FetchContent_MakeAvailable(nlohmann_json)

# miniz (zip for widgets): from vcpkg or the system, otherwise downloaded. ESP-IDF brings its own.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(INSTALL_PROJECT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(miniz
    GIT_REPOSITORY    https://github.com/richgel999/miniz.git
    GIT_TAG           3.0.2
    GIT_SHALLOW       TRUE
    SYSTEM
    FIND_PACKAGE_ARGS CONFIG
)
# miniz 3.0.2 asks for CMake 3.0, which CMake 4 no longer accepts; only for miniz
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(miniz)
unset(CMAKE_POLICY_VERSION_MINIMUM)
if(NOT TARGET miniz::miniz)
    add_library(miniz::miniz ALIAS miniz)
endif()

# Static modules
add_subdirectory(common)
add_subdirectory(logger)
add_subdirectory(downloader)
add_subdirectory(database)
add_subdirectory(httpd)
add_subdirectory(configuration)
add_subdirectory(rest_api)
add_subdirectory(smil)
add_subdirectory(content)
add_subdirectory(reports)
add_subdirectory(tasks)

if(GARLIC_CORE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Shared library 'core' made of all static modules
if(GARLIC_CORE_BUILD_SHARED)
    # CMake needs at least one source file; the library itself consists only of the modules.
    file(CONFIGURE
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/core.cpp"
        CONTENT "// generated: the shared library 'core' consists only of the static modules\n"
    )

    add_library(garlic_core SHARED "${CMAKE_CURRENT_BINARY_DIR}/generated/core.cpp")
    add_library(garlic::core ALIAS garlic_core)
    garlic_core_set_options(garlic_core)

    # WHOLE_ARCHIVE keeps all symbols of the modules, even if the shared library itself does not use them.
    target_link_libraries(garlic_core PUBLIC
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,garlic::core::common>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,garlic::core::logger>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,garlic::core::downloader>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,garlic::core::database>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,garlic::core::httpd>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,garlic::core::configuration>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,garlic::core::rest_api>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,garlic::core::smil>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,garlic::core::content>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,garlic::core::reports>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,garlic::core::tasks>"
    )

    set_target_properties(garlic_core PROPERTIES
        OUTPUT_NAME core
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
endif()
