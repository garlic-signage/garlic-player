# garlic-player: the core and the frontends in one project (e.g. for CLion). The old Qt 5 code in legacy/ is not
# part of it.
cmake_minimum_required(VERSION 3.25)

project(garlic-player LANGUAGES C CXX)

option(GARLIC_PLAYER_QWIDGET "Build the Qt Widgets player (src/player-ui/qwidget)" ON)
option(GARLIC_PLAYER_QML "Build the Qt Quick player (src/player-ui/qml)" ON)

add_subdirectory(src/core)
if(GARLIC_PLAYER_QWIDGET)
    add_subdirectory(src/player-ui/qwidget)
endif()
if(GARLIC_PLAYER_QML)
    add_subdirectory(src/player-ui/qml)
endif()
