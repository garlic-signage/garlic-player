add_executable(garlic_core_configuration_tests
    main.cpp
    test_configuration.cpp
    test_ini_file.cpp
)
garlic_core_set_options(garlic_core_configuration_tests TEST)

target_include_directories(garlic_core_configuration_tests PRIVATE ../support)

target_link_libraries(garlic_core_configuration_tests PRIVATE
    garlic::core::configuration
    doctest::doctest
)

add_test(NAME configuration COMMAND garlic_core_configuration_tests)
