configure_file(src/version.cpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/version.cpp" @ONLY)

add_library(garlic_core_common STATIC
    "${CMAKE_CURRENT_BINARY_DIR}/generated/version.cpp"
)
add_library(garlic::core::common ALIAS garlic_core_common)
garlic_core_set_options(garlic_core_common)

target_include_directories(garlic_core_common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
