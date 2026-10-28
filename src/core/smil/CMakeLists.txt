find_package(LibXml2 REQUIRED)

add_library(garlic_core_smil STATIC
    expr.cpp
    parser.cpp
    wallclock.cpp
)
add_library(garlic::core::smil ALIAS garlic_core_smil)
garlic_core_set_options(garlic_core_smil)

# Includes are written as "smil/parser.hpp"
target_include_directories(garlic_core_smil PUBLIC ${PROJECT_SOURCE_DIR})

# libxml2 only in the .cpp files, the headers are plain C++; common for the local time of wallclock values
target_link_libraries(garlic_core_smil PRIVATE LibXml2::LibXml2 garlic::core::common)
