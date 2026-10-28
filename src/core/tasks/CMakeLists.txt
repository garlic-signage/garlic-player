find_package(LibXml2 REQUIRED)

add_library(garlic_core_tasks STATIC
    task_list.cpp
    task_scheduler.cpp
)
add_library(garlic::core::tasks ALIAS garlic_core_tasks)
garlic_core_set_options(garlic_core_tasks)

# Includes are written as "tasks/task_scheduler.hpp"
target_include_directories(garlic_core_tasks PUBLIC ${PROJECT_SOURCE_DIR})

# libxml2 only in task_list.cpp
target_link_libraries(garlic_core_tasks
    PUBLIC  garlic::core::smil garlic::core::downloader garlic::core::configuration garlic::core::logger
    PRIVATE LibXml2::LibXml2
)
