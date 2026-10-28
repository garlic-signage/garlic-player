add_executable(garlic_core_tasks_tests
    main.cpp
    test_task_list.cpp
    test_task_scheduler.cpp
)
garlic_core_set_options(garlic_core_tasks_tests TEST)
target_include_directories(garlic_core_tasks_tests PRIVATE ../support ../downloader)
target_link_libraries(garlic_core_tasks_tests PRIVATE garlic::core::tasks doctest::doctest)
add_test(NAME tasks COMMAND garlic_core_tasks_tests)
