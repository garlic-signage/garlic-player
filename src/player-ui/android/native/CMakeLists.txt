# The native part of the Android player: the core and the JNI bridge (jni.cpp) as libgarlic-player.so, loaded by
# NativeCore.java. Built by ../build-native.sh for one ABI with the NDK and vcpkg, the libraries go to
# app/src/main/jniLibs/<abi>.
cmake_minimum_required(VERSION 3.31)

project(garlic_player_android VERSION 2.0.0 LANGUAGES C CXX)

set(GARLIC_CORE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../core ${CMAKE_CURRENT_BINARY_DIR}/core)

# nlohmann/json comes with the core (vcpkg or downloaded); the target is only visible in its directory
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json 3.11 CONFIG REQUIRED)
endif()

add_library(garlic-player SHARED
    jni.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../sdl/color.cpp # the SMIL colors like the sdl player
)
target_compile_features(garlic-player PRIVATE cxx_std_23)
target_include_directories(garlic-player PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../sdl)
target_link_libraries(garlic-player PRIVATE garlic::core nlohmann_json::nlohmann_json android log)
target_compile_options(garlic-player PRIVATE -Wall -Wextra -Wpedantic)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../player_version.cmake)
garlic_player_version(garlic-player)
