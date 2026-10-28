add_library(garlic_core_downloader STATIC
    download_queue.cpp
    downloader.cpp
    http_headers.cpp
)
add_library(garlic::core::downloader ALIAS garlic_core_downloader)
garlic_core_set_options(garlic_core_downloader)

# Includes are written as "downloader/http_transport.hpp"
target_include_directories(garlic_core_downloader PUBLIC ${PROJECT_SOURCE_DIR})

target_link_libraries(garlic_core_downloader PUBLIC garlic::core::common)

# libcurl is not available on WASM and ESP32, there other transports are used
if(GARLIC_CORE_WITH_CURL)
    find_package(CURL REQUIRED)
    target_sources(garlic_core_downloader PRIVATE curl_transport.cpp)
    target_link_libraries(garlic_core_downloader PUBLIC CURL::libcurl)
endif()
