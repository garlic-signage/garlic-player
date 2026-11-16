# sdl: the SDL3 frontend of the garlic-player on the core (PlayerRuntimeApi in a ThreadedRuntime), without Qt.
# Video and audio by libmpv (if found). Web pages: the WebView interface (web_view.hpp), CEF on Linux comes next.
cmake_minimum_required(VERSION 3.31)

project(sdl VERSION 2.0.0 LANGUAGES C CXX)

include(FetchContent)

# SDL3, SDL3_ttf, SDL3_image: taken from the system (or vcpkg) if there, otherwise downloaded. Debian Trixie is the
# oldest target: SDL3 3.2, SDL3_ttf 3.2, SDL3_image 3.2.
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    GIT_REPOSITORY    https://github.com/libsdl-org/SDL.git
    GIT_TAG           release-3.2.26
    GIT_SHALLOW       TRUE
    SYSTEM
    FIND_PACKAGE_ARGS 3.2 CONFIG
)
FetchContent_MakeAvailable(SDL3)

set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE) # only used when downloaded: FreeType and HarfBuzz with it
set(SDLTTF_HARFBUZZ ON CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3_ttf
    GIT_REPOSITORY    https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG           release-3.2.2
    GIT_SHALLOW       TRUE
    GIT_SUBMODULES    external/freetype external/harfbuzz
    SYSTEM
    FIND_PACKAGE_ARGS 3.2 CONFIG
)
FetchContent_MakeAvailable(SDL3_ttf)

# SDL3_image: PNG, JPEG, GIF, BMP, SVG built in (stb, nanosvg); WebP from the system (libwebp-dev)
set(SDLIMAGE_VENDORED OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3_image
    GIT_REPOSITORY    https://github.com/libsdl-org/SDL_image.git
    GIT_TAG           release-3.2.6
    GIT_SHALLOW       TRUE
    SYSTEM
    FIND_PACKAGE_ARGS 3.2 CONFIG
)
FetchContent_MakeAvailable(SDL3_image)

# nlohmann/json for the supervisor client: the core brings it (from the system or downloaded)
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json 3.11 CONFIG REQUIRED)
endif()

add_executable(sdl
    main.cpp
    paths.cpp
    player_window.cpp
    media.cpp
    color.cpp
    dialogs.cpp
    blank_screen.cpp
    supervisor_client.cpp
    web_view.cpp
    ui/controls.cpp
    ui/look.cpp
    ui/pointer_router.cpp
    ui/sdl_canvas.cpp
)
set_target_properties(sdl PROPERTIES OUTPUT_NAME garlic-player-sdl CXX_EXTENSIONS OFF)
target_compile_features(sdl PRIVATE cxx_std_23)
if(NOT MSVC)
    target_compile_options(sdl PRIVATE -Wall -Wextra -Wpedantic)
endif()
target_include_directories(sdl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
include(${CMAKE_CURRENT_SOURCE_DIR}/../player_version.cmake)
garlic_player_version(sdl)
target_link_libraries(sdl PRIVATE garlic::core SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image
                                  nlohmann_json::nlohmann_json)
if(NOT MSVC AND NOT APPLE) # Release without symbol table (strip)
    target_link_options(sdl PRIVATE $<$<CONFIG:Release>:-s>)
endif()

# libmpv for video and audio (render API with OpenGL; client API 2.0 = mpv 0.35, Debian Trixie has 0.40)
option(PLAYER_SDL_WITH_MPV "Video and audio by libmpv, if found" ON)
if(PLAYER_SDL_WITH_MPV)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(MPV IMPORTED_TARGET mpv>=2.0)
    endif()
    if(MPV_FOUND)
        target_sources(sdl PRIVATE mpv_media.cpp)
        target_compile_definitions(sdl PRIVATE PLAYER_SDL_WITH_MPV)
        target_link_libraries(sdl PRIVATE PkgConfig::MPV)
        message(STATUS "sdl player: video and audio by libmpv ${MPV_VERSION}")
    else()
        message(WARNING "sdl player: libmpv not found (e.g. libmpv-dev, mpv), built without video and audio")
    endif()
endif()

# The font of the menu and the dialogs next to the program, where main.cpp looks for it
add_custom_command(TARGET sdl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:sdl>/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/../qwidget/fonts/DejaVuSans.ttf
            $<TARGET_FILE_DIR:sdl>/fonts/DejaVuSans.ttf
)
