# garlic-player: the core and the frontends in one project (e.g. for CLion). The old Qt 5 code in legacy/ is not
# part of it.
cmake_minimum_required(VERSION 3.31)

project(garlic-player LANGUAGES C CXX)
if(APPLE)
    enable_language(OBJCXX) # the CEF wrapper and the SDL player have .mm files on macOS
endif()
option(GARLIC_PLAYER_SDL "Build the SDL3 player (src/player-ui/sdl)" ON)

enable_testing() # ctest in the build directory runs the tests of the frontends
add_subdirectory(src/core)
if(GARLIC_PLAYER_SDL)
    add_subdirectory(src/player-ui/sdl)
endif()
