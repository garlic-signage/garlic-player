# garlic-player: the core and the frontends in one project (e.g. for CLion). The old Qt 5 code in legacy/ is not
# part of it.
cmake_minimum_required(VERSION 3.25)

project(garlic-player LANGUAGES C CXX)

add_subdirectory(src/core)
add_subdirectory(src/player-widget)
