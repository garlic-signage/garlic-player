add_executable(garlic_core_downloader_tests
    main.cpp
    test_download_queue.cpp
    test_downloader.cpp
    test_http_transport.cpp
)
garlic_core_set_options(garlic_core_downloader_tests TEST)

target_include_directories(garlic_core_downloader_tests PRIVATE ../support)

target_link_libraries(garlic_core_downloader_tests PRIVATE
    garlic::core::downloader
    doctest::doctest
)

add_test(NAME downloader COMMAND garlic_core_downloader_tests)
