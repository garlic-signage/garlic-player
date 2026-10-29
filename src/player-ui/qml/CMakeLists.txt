# qml: the Qt Quick frontend of the garlic-player on the core, meant for iOS (and the desktop to try it). The same
# behaviour as qwidget; video and audio with Qt Multimedia, web pages with Qt WebView (the system WebView on mobiles).
cmake_minimum_required(VERSION 3.25)

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
target_compile_definitions(qml PRIVATE PLAYER_VERSION="${PROJECT_VERSION}-beta" QT_NO_KEYWORDS)
target_link_libraries(qml PRIVATE garlic::core Qt6::Quick Qt6::QuickControls2 Qt6::Multimedia Qt6::WebView)
if(NOT MSVC AND NOT APPLE) # Release without symbol table (strip)
    target_link_options(qml PRIVATE $<$<CONFIG:Release>:-s>)
endif()
