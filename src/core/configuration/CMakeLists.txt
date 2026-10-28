add_library(garlic_core_configuration STATIC
    configuration.cpp
    ini_file.cpp
)
add_library(garlic::core::configuration ALIAS garlic_core_configuration)
garlic_core_set_options(garlic_core_configuration)

# Includes are written as "configuration/configuration.hpp"
target_include_directories(garlic_core_configuration PUBLIC ${PROJECT_SOURCE_DIR})

target_link_libraries(garlic_core_configuration PUBLIC garlic::core::common garlic::core::httpd)
