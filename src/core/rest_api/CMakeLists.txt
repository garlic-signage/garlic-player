add_library(garlic_core_rest_api STATIC
    rest_api.cpp
)
add_library(garlic::core::rest_api ALIAS garlic_core_rest_api)
garlic_core_set_options(garlic_core_rest_api)

# Includes are written as "rest_api/rest_api.hpp"
target_include_directories(garlic_core_rest_api PUBLIC ${PROJECT_SOURCE_DIR})

target_link_libraries(garlic_core_rest_api PUBLIC
    garlic::core::configuration
    garlic::core::database
    garlic::core::httpd
    nlohmann_json::nlohmann_json
)
