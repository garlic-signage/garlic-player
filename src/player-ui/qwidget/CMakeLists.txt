# qwidget: the Qt Widgets frontend of the garlic-player on the core (PlayerRuntimeApi in a ThreadedRuntime)
cmake_minimum_required(VERSION 3.25)

project(qwidget VERSION 2.0.0 LANGUAGES CXX)

# Video and audio backends, chosen at runtime with --media-backend; at least one has to be built
option(PLAYER_WITH_QTAVPLAYER "QtAVPlayer (FFmpeg), the default backend" ON)
option(PLAYER_WITH_QTMULTIMEDIA "Qt Multimedia" ON)
option(PLAYER_WITH_VLC "libvlc" ON)
if(NOT PLAYER_WITH_QTAVPLAYER AND NOT PLAYER_WITH_QTMULTIMEDIA AND NOT PLAYER_WITH_VLC)
    message(FATAL_ERROR "qwidget needs at least one media backend (PLAYER_WITH_*)")
endif()

set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets WebEngineWidgets)
if(PLAYER_WITH_QTAVPLAYER OR PLAYER_WITH_QTMULTIMEDIA)
    find_package(Qt6 REQUIRED COMPONENTS Multimedia MultimediaWidgets)
endif()

add_executable(qwidget
    main.cpp
    main_window.cpp
    region.cpp
    media.cpp
    image.cpp
    web.cpp
    av_media.cpp
    media_player.cpp
    settings_dialog.cpp
    debug_dialog.cpp
)
set_target_properties(qwidget PROPERTIES OUTPUT_NAME garlic-player CXX_EXTENSIONS OFF)
target_compile_features(qwidget PRIVATE cxx_std_23)
if(NOT MSVC)
    target_compile_options(qwidget PRIVATE -Wall -Wextra -Wpedantic)
endif()
# QT_NO_KEYWORDS: the core has members like "slots", which the Qt macros would replace
target_compile_definitions(qwidget PRIVATE PLAYER_VERSION="${PROJECT_VERSION}-beta" QT_NO_KEYWORDS)
target_link_libraries(qwidget PRIVATE garlic::core Qt6::Widgets Qt6::WebEngineWidgets)
if(NOT MSVC AND NOT APPLE) # Release without symbol table (strip)
    target_link_options(qwidget PRIVATE $<$<CONFIG:Release>:-s>)
endif()

if(PLAYER_WITH_QTAVPLAYER)
    add_subdirectory(qtavplayer)
    target_sources(qwidget PRIVATE media_player_qtav.cpp)
    target_compile_definitions(qwidget PRIVATE PLAYER_WITH_QTAVPLAYER)
    target_link_libraries(qwidget PRIVATE qtavplayer Qt6::MultimediaWidgets)
endif()

if(PLAYER_WITH_QTMULTIMEDIA)
    target_sources(qwidget PRIVATE media_player_qtmultimedia.cpp)
    target_compile_definitions(qwidget PRIVATE PLAYER_WITH_QTMULTIMEDIA)
    target_link_libraries(qwidget PRIVATE Qt6::Multimedia Qt6::MultimediaWidgets)
endif()

if(PLAYER_WITH_VLC)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBVLC REQUIRED IMPORTED_TARGET libvlc)
    target_sources(qwidget PRIVATE media_player_vlc.cpp)
    target_compile_definitions(qwidget PRIVATE PLAYER_WITH_VLC)
    target_link_libraries(qwidget PRIVATE PkgConfig::LIBVLC)
endif()
