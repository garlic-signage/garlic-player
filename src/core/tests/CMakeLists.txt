include(FetchContent)

# doctest: taken from vcpkg or the system if available, otherwise downloaded. So the tests run on Linux and macOS
# without installing anything.
set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)

FetchContent_Declare(doctest
    GIT_REPOSITORY    https://github.com/doctest/doctest.git
    GIT_TAG           v2.4.12
    GIT_SHALLOW       TRUE
    SYSTEM            # warnings of doctest itself are not ours
    FIND_PACKAGE_ARGS CONFIG
)
FetchContent_MakeAvailable(doctest)

# One test executable per module, so every module is tested separately.
add_subdirectory(common)
add_subdirectory(logger)
add_subdirectory(downloader)
add_subdirectory(database)
add_subdirectory(httpd)
add_subdirectory(configuration)
add_subdirectory(rest_api)
