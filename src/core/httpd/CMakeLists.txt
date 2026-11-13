add_library(garlic_core_httpd STATIC
    router.cpp
)
add_library(garlic::core::httpd ALIAS garlic_core_httpd)
garlic_core_set_options(garlic_core_httpd)

# Includes are written as "httpd/router.hpp"
target_include_directories(garlic_core_httpd PUBLIC ${PROJECT_SOURCE_DIR})

target_link_libraries(garlic_core_httpd PUBLIC garlic::core::common)

# The server itself; settings, Request and Router are always built (the configuration uses HttpdSettings)
if(GARLIC_CORE_WITH_HTTPD)
    if(WIN32 OR APPLE)
        # Windows (vcpkg) and macOS (own build without pkg-config): find libmicrohttpd directly
        find_path(MHD_INCLUDE_DIR microhttpd.h REQUIRED)
        find_library(MHD_LIBRARY NAMES libmicrohttpd libmicrohttpd-dll microhttpd REQUIRED)
        add_library(PkgConfig::MHD INTERFACE IMPORTED)
        set_target_properties(PkgConfig::MHD PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${MHD_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES "${MHD_LIBRARY}"
        )
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(MHD REQUIRED IMPORTED_TARGET libmicrohttpd)
    endif()
    target_sources(garlic_core_httpd PRIVATE mhd_server.cpp)
    target_link_libraries(garlic_core_httpd PUBLIC PkgConfig::MHD)
endif()