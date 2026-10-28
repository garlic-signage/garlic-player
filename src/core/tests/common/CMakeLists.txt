add_executable(garlic_core_common_tests
    test_version.cpp
)
garlic_core_set_options(garlic_core_common_tests)

target_link_libraries(garlic_core_common_tests PRIVATE
    garlic::core::common
    doctest::doctest
)

target_compile_definitions(garlic_core_common_tests PRIVATE
    GARLIC_CORE_EXPECTED_VERSION="${PROJECT_VERSION}"
)

add_test(NAME common COMMAND garlic_core_common_tests)
