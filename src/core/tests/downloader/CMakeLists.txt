add_executable(garlic_core_downloader_tests
    test_downloader.cpp
)
garlic_core_set_options(garlic_core_downloader_tests)

target_link_libraries(garlic_core_downloader_tests PRIVATE
    garlic::core::downloader
    doctest::doctest
)

add_test(NAME downloader COMMAND garlic_core_downloader_tests)
