# sdl: the SDL3 frontend of the garlic-player on the core (PlayerRuntimeApi in a ThreadedRuntime), without Qt.
# Video and audio by libmpv (if found), web pages by CEF on Linux (PLAYER_SDL_WITH_CEF), behind the WebView interface.
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
    place.cpp
    color.cpp
    dialogs.cpp
    blank_screen.cpp
    supervisor_client.cpp
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

# CEF for web pages, off-screen rendered into textures. Linux only (x86_64, aarch64); the other platforms get their
# native web views later. The minimal binary distribution is downloaded (about 350 MB) and its C++ wrapper built.
option(PLAYER_SDL_WITH_CEF "Web pages by CEF (Linux; downloads the CEF binary distribution)" OFF)
set(_sdl_with_cef OFF)
if(PLAYER_SDL_WITH_CEF)
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _cpu)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(WARNING "sdl player: CEF only on Linux, built without web pages")
    elseif(_cpu MATCHES "^(x86_64|amd64)$")
        set(_cef_platform linux64)
        set(_cef_sha1 0b4c2189e3b93033bbef13add1a86f66849c1e62)
        set(_sdl_with_cef ON)
    elseif(_cpu MATCHES "^(aarch64|arm64)$")
        set(_cef_platform linuxarm64)
        set(_cef_sha1 ea8f06c33514f9f3d75ca003437f9ac45223e07d)
        set(_sdl_with_cef ON)
    else()
        message(WARNING "sdl player: no CEF for ${CMAKE_SYSTEM_PROCESSOR}, built without web pages")
    endif()
endif()
if(_sdl_with_cef)
    set(CEF_VERSION "156.0.30+g6028375+chromium-156.0.8078.96")
    string(REPLACE "+" "%2B" _cef_url_version "${CEF_VERSION}")
    FetchContent_Declare(cef
        URL               https://cef-builds.spotifycdn.com/cef_binary_${_cef_url_version}_${_cef_platform}_minimal.tar.bz2
        URL_HASH          SHA1=${_cef_sha1}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR     not-added # only the files: the wrapper is added below
    )
    FetchContent_MakeAvailable(cef)
    set(CEF_ROOT ${cef_SOURCE_DIR})
    list(APPEND CMAKE_MODULE_PATH ${CEF_ROOT}/cmake)
    find_package(CEF REQUIRED)
    add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} ${CMAKE_CURRENT_BINARY_DIR}/libcef_dll_wrapper)

    target_sources(sdl PRIVATE cef_web_view.cpp)
    target_compile_definitions(sdl PRIVATE PLAYER_SDL_WITH_CEF)
    target_include_directories(sdl SYSTEM PRIVATE ${CEF_ROOT})
    target_link_libraries(sdl PRIVATE libcef_dll_wrapper ${CEF_ROOT}/Release/libcef.so)
    set_target_properties(sdl PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN") # libcef.so next to it

    # libcef.so, its resources and locales next to the program, where CEF looks for them
    foreach(_file ${CEF_BINARY_FILES})
        add_custom_command(TARGET sdl POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                           ${CEF_ROOT}/Release/${_file} $<TARGET_FILE_DIR:sdl>/${_file})
    endforeach()
    foreach(_file ${CEF_RESOURCE_FILES})
        if(IS_DIRECTORY ${CEF_ROOT}/Resources/${_file})
            add_custom_command(TARGET sdl POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory
                               ${CEF_ROOT}/Resources/${_file} $<TARGET_FILE_DIR:sdl>/${_file})
        else()
            add_custom_command(TARGET sdl POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                               ${CEF_ROOT}/Resources/${_file} $<TARGET_FILE_DIR:sdl>/${_file})
        endif()
    endforeach()
    message(STATUS "sdl player: web pages by CEF ${CEF_VERSION} (${_cef_platform})")
else()
    target_sources(sdl PRIVATE web_view.cpp) # no web engine: web pages fail, the core goes on
endif()

# The font of the menu and the dialogs next to the program, where main.cpp looks for it
add_custom_command(TARGET sdl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:sdl>/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/../qwidget/fonts/DejaVuSans.ttf
            $<TARGET_FILE_DIR:sdl>/fonts/DejaVuSans.ttf
)

option(GARLIC_PLAYER_SDL_TESTS "Build the tests of the SDL player (doctest)" ON)
if(GARLIC_PLAYER_SDL_TESTS)
    add_subdirectory(tests)
endif()
