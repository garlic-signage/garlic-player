add_library(garlic_core_content STATIC
    index_manager.cpp
    media_manager.cpp
    widget.cpp
)
add_library(garlic::core::content ALIAS garlic_core_content)
garlic_core_set_options(garlic_core_content)

# Includes are written as "content/media_manager.hpp"
target_include_directories(garlic_core_content PUBLIC ${PROJECT_SOURCE_DIR})

# miniz only in widget.cpp
target_link_libraries(garlic_core_content
    PUBLIC  garlic::core::smil garlic::core::downloader garlic::core::configuration garlic::core::logger
    PRIVATE miniz::miniz
)
