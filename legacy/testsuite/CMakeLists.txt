cmake_minimum_required(VERSION 3.5)
project(GarlicLibTests LANGUAGES CXX)
set (CMAKE_CXX_STANDARD 17)

#some extra mimimi for mac
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64") # alternatives x86_64;arm64
    set(Qt5_DIR "/Users/niko/Qt/5.15.13/clang_64/lib/cmake/Qt5")
    set(CMAKE_PREFIX_PATH "/Users/niko/Qt/5.15.13/clang_64")
    set(GTEST_INCLUDE_DIR /opt/local/include)
    set(GMOCK_INCLUDE_DIR /opt/local/include)
    set(GTEST_LIB_DIR /opt/local/lib)
    set(GMOCK_LIB_DIR /opt/local/lib)
    include_directories(${GTEST_INCLUDE_DIR} ${GMOCK_INCLUDE_DIR})
    link_directories(${GTEST_LIB_DIR} ${GMOCK_LIB_DIR})
endif()

# add QTest and gmock
find_package(Qt5Test REQUIRED)
find_package(GTest REQUIRED)
find_package(Qt5 COMPONENTS Xml XmlPatterns REQUIRED)

# Automoc aktivieren
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

enable_testing()

include_directories(../src/garlic-lib)
include_directories(../src/garlic-lib/smilparser)
include_directories(../src/garlic-lib/smilparser/trigger)
include_directories(../src/garlic-lib/smilparser/timings)
include_directories(../src/garlic-lib/tools)
include_directories(../src/garlic-lib/tools/reboot)
link_directories(../src/lib/)

include_directories(../../src/core)

file(GLOB_RECURSE TEST_SOURCES garlic-lib/*.cpp)
file(GLOB_RECURSE TEST_HEADERS garlic-lib/*.hpp)
add_executable(runTests ${TEST_SOURCES} ${TEST_HEADERS} main.cpp base_testsuite.cpp base_testsuite.hpp)

target_link_libraries(runTests Qt5::Test GTest::GTest GTest::Main gmock gmock_main libgarlic.so Qt5::Xml Qt5::XmlPatterns)

# register tests
add_test(NAME runTests COMMAND runTests)
