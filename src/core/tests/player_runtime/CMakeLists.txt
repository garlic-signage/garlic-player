add_executable(garlic_core_player_runtime_tests
    main.cpp
    test_player_runtime_api.cpp
    test_reboot_schedule.cpp
)
garlic_core_set_options(garlic_core_player_runtime_tests TEST)
target_include_directories(garlic_core_player_runtime_tests PRIVATE ../support ../downloader)
target_link_libraries(garlic_core_player_runtime_tests PRIVATE garlic::core::player_runtime doctest::doctest)
add_test(NAME player_runtime COMMAND garlic_core_player_runtime_tests)
