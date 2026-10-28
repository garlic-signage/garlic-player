find_package(SQLite3 REQUIRED)

add_library(garlic_core_database STATIC
    inventory_table.cpp
)
add_library(garlic::core::database ALIAS garlic_core_database)
garlic_core_set_options(garlic_core_database)

# Includes are written as "database/inventory_table.hpp"
target_include_directories(garlic_core_database PUBLIC ${PROJECT_SOURCE_DIR})

# downloader for the Inventory interface
target_link_libraries(garlic_core_database PUBLIC garlic::core::downloader SQLite::SQLite3)
