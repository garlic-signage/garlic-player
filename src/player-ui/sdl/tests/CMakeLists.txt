# The parts of the SDL player that need no window: place() for fit and mediaAlign of images and videos, the text
# field of the dialogs.
include(FetchContent)
if(NOT TARGET doctest::doctest)
    # like the tests of the core: from vcpkg or the system if available, otherwise downloaded
    set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
    set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(doctest
        GIT_REPOSITORY    https://github.com/doctest/doctest.git
        GIT_TAG           v2.4.12
        GIT_SHALLOW       TRUE
        SYSTEM
        FIND_PACKAGE_ARGS CONFIG
    )
    set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
    FetchContent_MakeAvailable(doctest)
    unset(CMAKE_POLICY_VERSION_MINIMUM)
endif()

add_executable(player_sdl_tests main.cpp test_place.cpp test_text_field.cpp ../place.cpp ../ui/controls.cpp
                                ../ui/look.cpp)
target_include_directories(player_sdl_tests PRIVATE ..)
target_link_libraries(player_sdl_tests PRIVATE doctest::doctest SDL3::Headers)
target_compile_features(player_sdl_tests PRIVATE cxx_std_23)
set_target_properties(player_sdl_tests PROPERTIES CXX_EXTENSIONS OFF)
add_test(NAME player_sdl COMMAND player_sdl_tests)
