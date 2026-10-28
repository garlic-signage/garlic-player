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
    GIT_TAG           3.1.1 # 3.1.1 asks for CMake 3.5...3.12, so neither CMake 3.x nor 4 warns; 3.1.2 went back to 3.5
    GIT_SHALLOW       TRUE
    SYSTEM
    FIND_PACKAGE_ARGS CONFIG
)
FetchContent_MakeAvailable(miniz)
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
if(_garlic_core_native)
    add_subdirectory(system)
endif()
add_subdirectory(player_runtime)

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
    # PRIVATE: the modules are only in the shared library. PUBLIC would hand them on to every user of garlic::core,
    # which would link them a second time into its own binary.
    target_link_libraries(garlic_core PRIVATE
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
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,garlic::core::system>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,garlic::core::player_runtime>"
    )

    # What users of garlic::core need for the headers: the include directory and nlohmann/json (json_fwd.hpp)
    target_include_directories(garlic_core PUBLIC ${PROJECT_SOURCE_DIR})
    target_link_libraries(garlic_core PUBLIC nlohmann_json::nlohmann_json)

    # Release without symbol table (strip); MSVC keeps symbols in the .pdb anyway, Apple's linker strips differently
    if(NOT MSVC AND NOT APPLE)
        target_link_options(garlic_core PRIVATE $<$<CONFIG:Release>:-s>)
    endif()

    set_target_properties(garlic_core PROPERTIES
        OUTPUT_NAME core
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
endif()
