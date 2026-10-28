add_executable(garlic_core_smil_tests
    main.cpp
    test_head.cpp
    test_media.cpp
    test_containers.cpp
    test_excl.cpp
    test_triggers.cpp
    test_wallclock.cpp
    test_expr.cpp
    test_engine.cpp
    test_engine_containers.cpp
    test_engine_timing.cpp
    test_engine_excl.cpp
    test_engine_triggers.cpp
)
garlic_core_set_options(garlic_core_smil_tests TEST)

target_include_directories(garlic_core_smil_tests PRIVATE ../support)

# SMIL files of the tests
target_compile_definitions(garlic_core_smil_tests PRIVATE
    GARLIC_CORE_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/../data/smil"
)

target_link_libraries(garlic_core_smil_tests PRIVATE
    garlic::core::smil
    doctest::doctest
)

add_test(NAME smil COMMAND garlic_core_smil_tests)
