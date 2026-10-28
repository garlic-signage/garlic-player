add_library(garlic_core_reports STATIC
    report_xml.cpp
    reporter.cpp
)
add_library(garlic::core::reports ALIAS garlic_core_reports)
garlic_core_set_options(garlic_core_reports)

# Includes are written as "reports/reporter.hpp"
target_include_directories(garlic_core_reports PUBLIC ${PROJECT_SOURCE_DIR})

target_link_libraries(garlic_core_reports PUBLIC
    garlic::core::smil garlic::core::database garlic::core::configuration garlic::core::logger
)
