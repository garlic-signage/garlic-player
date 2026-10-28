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
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MHD REQUIRED IMPORTED_TARGET libmicrohttpd)
    target_sources(garlic_core_httpd PRIVATE mhd_server.cpp)
    target_link_libraries(garlic_core_httpd PUBLIC PkgConfig::MHD)
endif()
