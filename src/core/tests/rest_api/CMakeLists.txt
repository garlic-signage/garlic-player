add_executable(garlic_core_rest_api_tests
    main.cpp
    test_rest_api.cpp
)
garlic_core_set_options(garlic_core_rest_api_tests TEST)

target_include_directories(garlic_core_rest_api_tests PRIVATE ../support)

target_link_libraries(garlic_core_rest_api_tests PRIVATE
    garlic::core::rest_api
    doctest::doctest
)

add_test(NAME rest_api COMMAND garlic_core_rest_api_tests)
