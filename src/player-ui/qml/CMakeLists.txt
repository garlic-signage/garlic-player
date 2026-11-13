# qml: the Qt Quick frontend of the garlic-player on the core, for Android, meant for iOS (and the desktop to try it). The same
# behaviour as qwidget; video and audio with Qt Multimedia, web pages with Qt WebView (the system WebView on mobiles).
cmake_minimum_required(VERSION 3.31)

project(qml VERSION 2.0.0 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Multimedia WebView)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(qml
    main.cpp
    player.cpp
    region_model.cpp
    media_model.cpp
)
# The import scan links the QML plugins of a static Qt (iOS). A shared Qt loads them at runtime; Debian's Qt 6.8
# scans anyway and warns about every plugin, so it is switched off there.
if(QT6_IS_SHARED_LIBS_BUILD)
    set(import_scan NO_IMPORT_SCAN)
endif()
qt_add_qml_module(qml
    URI Garlic
    VERSION 1.0
    ${import_scan}
    SOURCES player.hpp
    QML_FILES
        Main.qml
        RegionView.qml
        MediaView.qml
        PlayerMenu.qml
        SettingsDialog.qml
        DebugDialog.qml
)
set_target_properties(qml PROPERTIES OUTPUT_NAME garlic-player-qml CXX_EXTENSIONS OFF)
target_compile_features(qml PRIVATE cxx_std_23)
if(NOT MSVC)
    target_compile_options(qml PRIVATE -Wall -Wextra -Wpedantic)
endif()
# QT_NO_KEYWORDS: the core has members like "slots", which the Qt macros would replace
target_compile_definitions(qml PRIVATE QT_NO_KEYWORDS)
include(${CMAKE_CURRENT_SOURCE_DIR}/../player_version.cmake)
garlic_player_version(qml)
target_link_libraries(qml PRIVATE garlic::core Qt6::Quick Qt6::QuickControls2 Qt6::Multimedia Qt6::WebView)
if(NOT MSVC AND NOT APPLE) # Release without symbol table (strip)
    target_link_options(qml PRIVATE $<$<CONFIG:Release>:-s>)
endif()

# Android: the package of the legacy player, so launcher 1.x and Philips start and update it and an update keeps the
# settings. versionCode above the legacy one (671): 2.0.0-beta.566 → 20000566, 2.0.0-dev → 20000000.
# targetSdk 29: the shared storage with WRITE_EXTERNAL_STORAGE also on Android 11+ (see android/AndroidManifest.xml).
if(ANDROID)
    target_sources(qml PRIVATE android_launcher.hpp android_launcher.cpp)
    set(_build 0) # the number of the build server after the last dot, e.g. 2.0.0-beta.566
    if(GARLIC_PLAYER_VERSION MATCHES "-.*\\.([0-9]+)$")
        set(_build ${CMAKE_MATCH_1})
    endif()
    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _version "${GARLIC_PLAYER_VERSION}")
    math(EXPR _version_code
         "${CMAKE_MATCH_1} * 10000000 + ${CMAKE_MATCH_2} * 100000 + ${CMAKE_MATCH_3} * 1000 + ${_build} % 1000")
    set_target_properties(qml PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_PACKAGE_NAME "com.sagiadinos.garlic.player"
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 29
        QT_ANDROID_VERSION_NAME "${GARLIC_PLAYER_VERSION}"
        QT_ANDROID_VERSION_CODE ${_version_code}
    )
endif()
