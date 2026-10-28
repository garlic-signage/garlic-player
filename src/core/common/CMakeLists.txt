configure_file(version.cpp.in "${CMAKE_CURRENT_BINARY_DIR}/version.cpp" @ONLY)

add_library(garlic_core_common STATIC
    "${CMAKE_CURRENT_BINARY_DIR}/version.cpp"
    md5.cpp
    memory_file_system.cpp
    std_file_system.cpp
    system_clock.cpp
)
add_library(garlic::core::common ALIAS garlic_core_common)
garlic_core_set_options(garlic_core_common)

# Includes are written as "common/clock.hpp"
target_include_directories(garlic_core_common PUBLIC ${PROJECT_SOURCE_DIR})
