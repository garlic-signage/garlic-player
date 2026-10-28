add_library(garlic_core_downloader STATIC
    http_headers.cpp
)
add_library(garlic::core::downloader ALIAS garlic_core_downloader)
garlic_core_set_options(garlic_core_downloader)

# Includes are written as "downloader/http_transport.hpp"
target_include_directories(garlic_core_downloader PUBLIC ${PROJECT_SOURCE_DIR})
