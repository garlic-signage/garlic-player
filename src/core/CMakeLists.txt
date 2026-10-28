cmake_minimum_required(VERSION 3.25)

project(garlic-core
    VERSION 0.1.0
    DESCRIPTION "Qt-free core library of the garlic-player"
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GarlicCoreOptions)

# No dynamic libraries on WASM and ESP32: the static modules are linked directly there.
if(EMSCRIPTEN OR ESP_PLATFORM)
    set(_garlic_core_shared_default OFF)
else()
    set(_garlic_core_shared_default ON)
endif()

option(GARLIC_CORE_BUILD_SHARED "Build the shared library 'core' from all static modules" ${_garlic_core_shared_default})
option(GARLIC_CORE_WITH_CURL "Build the CurlTransport (libcurl)" ${_garlic_core_shared_default})
option(GARLIC_CORE_BUILD_TESTS "Build the unit tests of the modules" ${PROJECT_IS_TOP_LEVEL})

# Static modules are linked into a shared library later, so they need PIC.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Static modules
add_subdirectory(common)
add_subdirectory(downloader)

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
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,garlic::core::downloader>"
    )

    set_target_properties(garlic_core PROPERTIES
        OUTPUT_NAME core
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
endif()
