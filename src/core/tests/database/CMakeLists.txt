add_executable(garlic_core_database_tests
    main.cpp
    test_inventory_table.cpp
)
garlic_core_set_options(garlic_core_database_tests TEST)

target_include_directories(garlic_core_database_tests PRIVATE ../support)

target_link_libraries(garlic_core_database_tests PRIVATE
    garlic::core::database
    doctest::doctest
)

add_test(NAME database COMMAND garlic_core_database_tests)
