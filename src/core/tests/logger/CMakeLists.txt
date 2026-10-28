add_executable(garlic_core_logger_tests
    main.cpp
    test_logger.cpp
)
garlic_core_set_options(garlic_core_logger_tests TEST)

target_include_directories(garlic_core_logger_tests PRIVATE ../support)

target_link_libraries(garlic_core_logger_tests PRIVATE
    garlic::core::logger
    doctest::doctest
)

add_test(NAME logger COMMAND garlic_core_logger_tests)
