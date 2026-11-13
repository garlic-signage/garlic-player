# supervisor: what the qml and the qwidget player share with their supervisor (launcher, garlic-daemon,
# garlic-service): the client of the supervisor protocol (src/core/docs/supervisor_protocol.md), adapi:blankScreen and
# what a supervisor asks of the core.
cmake_minimum_required(VERSION 3.31)

project(player_supervisor LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network)

add_library(player_supervisor STATIC
    screen_control.hpp
    device_control.hpp
    blank_screen.hpp
    blank_screen.cpp
    supervisor_client.hpp
    supervisor_client.cpp
    runtime_requests.hpp
    runtime_requests.cpp
)
set_target_properties(player_supervisor PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF)
target_compile_features(player_supervisor PUBLIC cxx_std_23)
if(NOT MSVC)
    target_compile_options(player_supervisor PRIVATE -Wall -Wextra -Wpedantic)
endif()
target_compile_definitions(player_supervisor PUBLIC QT_NO_KEYWORDS) # the core has members like "slots"
target_include_directories(player_supervisor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(player_supervisor PUBLIC garlic::core Qt6::Core Qt6::Network)

option(GARLIC_PLAYER_SUPERVISOR_TESTS "Build the tests of the supervisor client (Qt Test)" ${PROJECT_IS_TOP_LEVEL})
if(GARLIC_PLAYER_SUPERVISOR_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
