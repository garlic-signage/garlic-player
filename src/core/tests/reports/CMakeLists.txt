add_executable(garlic_core_reports_tests
    main.cpp
    test_report_xml.cpp
    test_reporter.cpp
)
garlic_core_set_options(garlic_core_reports_tests TEST)
target_include_directories(garlic_core_reports_tests PRIVATE ../support ../downloader)
target_link_libraries(garlic_core_reports_tests PRIVATE garlic::core::reports doctest::doctest)
add_test(NAME reports COMMAND garlic_core_reports_tests)
