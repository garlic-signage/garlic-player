add_library(garlic_core_logger STATIC
    logger.cpp
)
add_library(garlic::core::logger ALIAS garlic_core_logger)
garlic_core_set_options(garlic_core_logger)

# Includes are written as "logger/logger.hpp"
target_include_directories(garlic_core_logger PUBLIC ${PROJECT_SOURCE_DIR})

target_link_libraries(garlic_core_logger PUBLIC garlic::core::common)
