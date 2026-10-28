# garlic-player: the core and the frontends in one project (e.g. for CLion)
cmake_minimum_required(VERSION 3.25)

project(garlic-player LANGUAGES C CXX)

add_subdirectory(core)
add_subdirectory(player-widget)
