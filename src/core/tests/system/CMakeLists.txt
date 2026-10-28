add_executable(garlic_core_system_tests
    main.cpp
    test_system_info.cpp
)
garlic_core_set_options(garlic_core_system_tests TEST)
target_link_libraries(garlic_core_system_tests PRIVATE garlic::core::system doctest::doctest)
add_test(NAME system COMMAND garlic_core_system_tests)
