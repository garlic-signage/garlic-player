find_package(SQLite3 REQUIRED)
if(NOT TARGET SQLite3::SQLite3) # CMake < 4.3, where SQLite::SQLite3 is the only name
    add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
endif()

add_library(garlic_core_database STATIC
    inventory_table.cpp
)
add_library(garlic::core::database ALIAS garlic_core_database)
garlic_core_set_options(garlic_core_database)

# Includes are written as "database/inventory_table.hpp"
target_include_directories(garlic_core_database PUBLIC ${PROJECT_SOURCE_DIR})

# downloader for the Inventory interface
target_link_libraries(garlic_core_database PUBLIC garlic::core::downloader SQLite3::SQLite3)
