add_library(garlic_core_downloader STATIC
    src/http_headers.cpp
)
add_library(garlic::core::downloader ALIAS garlic_core_downloader)
garlic_core_set_options(garlic_core_downloader)

target_include_directories(garlic_core_downloader PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
