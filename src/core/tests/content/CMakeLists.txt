add_executable(garlic_core_content_tests
    main.cpp
    test_index_manager.cpp
    test_media_manager.cpp
    test_widget.cpp
)
garlic_core_set_options(garlic_core_content_tests TEST)
target_include_directories(garlic_core_content_tests PRIVATE ../support ../downloader)
# miniz builds the zip archives of the widget tests
target_link_libraries(garlic_core_content_tests PRIVATE garlic::core::content miniz::miniz doctest::doctest)
add_test(NAME content COMMAND garlic_core_content_tests)
