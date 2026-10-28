add_executable(garlic_core_httpd_tests
    main.cpp
    test_router.cpp
)
garlic_core_set_options(garlic_core_httpd_tests TEST)

target_link_libraries(garlic_core_httpd_tests PRIVATE
    garlic::core::httpd
    doctest::doctest
)

add_test(NAME httpd COMMAND garlic_core_httpd_tests)

# server tests use a raw socket client (POSIX only)
if(GARLIC_CORE_WITH_HTTPD AND NOT WIN32)
    target_sources(garlic_core_httpd_tests PRIVATE test_mhd_server.cpp)
endif()
