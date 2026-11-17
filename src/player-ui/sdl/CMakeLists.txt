# sdl: the SDL3 frontend of the garlic-player on the core (PlayerRuntimeApi in a ThreadedRuntime), without Qt.
# Video and audio by libmpv (if found), web pages by CEF (PLAYER_SDL_WITH_CEF), behind the WebView interface.
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
    file_url.cpp
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

# Windows: no console window (SDL_main.h gives WinMain), like the qwidget player; the manifest declares Windows 10/11,
# which CEF needs to see the real version. windows.h without the macros min and max.
if(WIN32)
    set_target_properties(sdl PROPERTIES WIN32_EXECUTABLE ON)
    target_sources(sdl PRIVATE windows/garlic-player-sdl.manifest)
    target_compile_definitions(sdl PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# macOS: an app bundle with icon and bundle id, like the qwidget player. SDL_GetBasePath() is Contents/Resources there:
# the files main.cpp looks for go there, next to the program everywhere else.
if(APPLE)
    set(_garlic_icon ${CMAKE_CURRENT_SOURCE_DIR}/../../../deployment/macx/garlic-player.icns)
    target_sources(sdl PRIVATE ${_garlic_icon})
    set_source_files_properties(${_garlic_icon} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(sdl PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_BUNDLE_NAME garlic-player-sdl
        MACOSX_BUNDLE_ICON_FILE garlic-player.icns
        MACOSX_BUNDLE_GUI_IDENTIFIER com.garlic-signage.garlic-player-sdl
        BUILD_RPATH "@executable_path/../Frameworks" # libmpv and its libraries
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
    set(_sdl_resource_dir $<TARGET_BUNDLE_CONTENT_DIR:sdl>/Resources)
else()
    set(_sdl_resource_dir $<TARGET_FILE_DIR:sdl>)
endif()

# libmpv for video and audio (render API with OpenGL; client API 2.0 = mpv 0.35, Debian Trixie has 0.40).
# Linux: from pkg-config. Windows: the official libmpv of mpv is downloaded (lgpl, one DLL with FFmpeg and the rest,
# include/mpv/*.h, libmpv.dll.a), or another one from MPV_DIR (same layout, mpv.def optional). macOS: the prebuilt
# libmpv of media_kit (github.com/media-kit/libmpv-darwin-build, mpv 0.36 with FFmpeg and libass, every library its
# own dylib with @rpath) is downloaded, the headers from the sources of mpv 0.36; the dylibs go into
# Contents/Frameworks of the bundle.
option(PLAYER_SDL_WITH_MPV "Video and audio by libmpv, if found" ON)
set(MPV_DIR "" CACHE PATH "Folder of libmpv instead of pkg-config or the download (include/mpv, libmpv*.dll)")
if(PLAYER_SDL_WITH_MPV)
    set(_mpv_found OFF)
    if(APPLE)
        FetchContent_Declare(libmpv_darwin
            URL      https://github.com/media-kit/libmpv-darwin-build/releases/download/v0.7.3/libmpv-libs_v0.7.3_macos-arm64-video-full.tar.gz
            URL_HASH SHA256=9bb168ec908b4801f4231f411e3278a0aea644a2b03ce375c880d2813ab7f949
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_Declare(mpv_headers
            URL           https://github.com/mpv-player/mpv/archive/refs/tags/v0.36.0.tar.gz
            URL_HASH      SHA256=29abc44f8ebee013bb2f9fe14d80b30db19b534c679056e4851ceadf5a5e8bf6
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            SOURCE_SUBDIR not-added # only the headers
        )
        FetchContent_MakeAvailable(libmpv_darwin mpv_headers)
        file(COPY ${mpv_headers_SOURCE_DIR}/libmpv/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/mpv-include/mpv
             FILES_MATCHING PATTERN "*.h")
        set(MPV_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/mpv-include)
        set(MPV_LIBRARY ${libmpv_darwin_SOURCE_DIR}/libmpv.dylib)
        file(GLOB _mpv_dylibs ${libmpv_darwin_SOURCE_DIR}/*.dylib)
        add_custom_command(TARGET sdl POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory
                           $<TARGET_BUNDLE_CONTENT_DIR:sdl>/Frameworks)
        foreach(_dylib ${_mpv_dylibs}) # found through @rpath (@executable_path/../Frameworks)
            add_custom_command(TARGET sdl POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_dylib}
                               $<TARGET_BUNDLE_CONTENT_DIR:sdl>/Frameworks/)
        endforeach()
        set(_mpv_found ON)
        message(STATUS "sdl player: video and audio by libmpv 0.36 of media_kit")
    elseif(NOT WIN32 AND NOT MPV_DIR)
        find_package(PkgConfig)
        if(PkgConfig_FOUND)
            pkg_check_modules(MPV IMPORTED_TARGET mpv>=2.0)
        endif()
        if(MPV_FOUND)
            target_link_libraries(sdl PRIVATE PkgConfig::MPV)
            set(_mpv_found ON)
            message(STATUS "sdl player: video and audio by libmpv ${MPV_VERSION}")
        endif()
    else()
        set(_mpv_dir ${MPV_DIR})
        if(NOT _mpv_dir) # Windows
            FetchContent_Declare(libmpv_windows
                URL      https://github.com/mpv-player/mpv/releases/download/v0.42.0/libmpv-v0.42.0-x86_64-w64-mingw32-lgpl.zip
                URL_HASH SHA256=8e5f77430efc53ef175804ba6618527e3af0448a0dcabf04d3cc72659d1a6fce
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            )
            FetchContent_MakeAvailable(libmpv_windows)
            set(_mpv_dir ${libmpv_windows_SOURCE_DIR})
        endif()
        find_path(MPV_INCLUDE_DIR mpv/render_gl.h HINTS ${_mpv_dir} PATH_SUFFIXES include NO_DEFAULT_PATH)
        file(GLOB _mpv_dlls ${_mpv_dir}/libmpv*.dll ${_mpv_dir}/bin/libmpv*.dll)
        if(MSVC AND _mpv_dlls AND MPV_INCLUDE_DIR)
            # MSVC: an import library (lib.exe) for the DLL, from mpv.def or, without it, from the functions the
            # headers export (MPV_EXPORT), the same as the exports of the DLL
            set(_mpv_def ${_mpv_dir}/mpv.def)
            if(NOT EXISTS ${_mpv_def})
                set(_mpv_def ${CMAKE_CURRENT_BINARY_DIR}/mpv.def)
                file(GLOB _mpv_headers ${MPV_INCLUDE_DIR}/mpv/*.h)
                set(_mpv_exports "")
                foreach(_header ${_mpv_headers})
                    file(READ ${_header} _content)
                    string(REGEX MATCHALL "MPV_EXPORT[^;{#]*[ *]mpv_[a-z0-9_]+\\(" _declarations "${_content}")
                    foreach(_declaration ${_declarations})
                        string(REGEX REPLACE ".*[ *](mpv_[a-z0-9_]+)\\($" "\\1" _name "${_declaration}")
                        list(APPEND _mpv_exports ${_name})
                    endforeach()
                endforeach()
                list(JOIN _mpv_exports "\n" _mpv_exports)
                file(WRITE ${_mpv_def} "EXPORTS\n${_mpv_exports}\n")
            endif()
            list(GET _mpv_dlls 0 _mpv_dll)
            get_filename_component(_mpv_dll_name ${_mpv_dll} NAME)
            set(MPV_LIBRARY ${CMAKE_CURRENT_BINARY_DIR}/mpv.lib)
            execute_process(COMMAND ${CMAKE_AR} /nologo /def:${_mpv_def} /name:${_mpv_dll_name} /machine:x64
                                    /out:${MPV_LIBRARY}
                            RESULT_VARIABLE _mpv_lib_result)
            if(NOT _mpv_lib_result EQUAL 0)
                unset(MPV_LIBRARY)
            endif()
        else()
            find_library(MPV_LIBRARY NAMES mpv libmpv libmpv.dll.a HINTS ${_mpv_dir} PATH_SUFFIXES lib)
        endif()
        if(MPV_INCLUDE_DIR AND MPV_LIBRARY)
            foreach(_dll ${_mpv_dlls}) # next to the program
                add_custom_command(TARGET sdl POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_dll}
                                   $<TARGET_FILE_DIR:sdl>)
            endforeach()
            set(_mpv_found ON)
            message(STATUS "sdl player: video and audio by libmpv from ${_mpv_dir}")
        endif()
    endif()
    if(_mpv_found)
        target_sources(sdl PRIVATE mpv_media.cpp)
        target_compile_definitions(sdl PRIVATE PLAYER_SDL_WITH_MPV)
        if(MPV_LIBRARY)
            target_include_directories(sdl SYSTEM PRIVATE ${MPV_INCLUDE_DIR})
            target_link_libraries(sdl PRIVATE ${MPV_LIBRARY})
        endif()
    else()
        message(WARNING "sdl player: libmpv not found (libmpv-dev; MPV_DIR), built without video and audio")
    endif()
endif()

# CEF for web pages, off-screen rendered into textures: Linux (x86_64, aarch64), Windows (x64), macOS (arm64). The
# minimal binary distribution is downloaded (about 350 MB) and its C++ wrapper built.
option(PLAYER_SDL_WITH_CEF "Web pages by CEF (downloads the CEF binary distribution)" OFF)
set(_sdl_with_cef OFF)
if(PLAYER_SDL_WITH_CEF)
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _cpu)
    if(APPLE AND CMAKE_OSX_ARCHITECTURES)
        string(TOLOWER "${CMAKE_OSX_ARCHITECTURES}" _cpu)
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND _cpu MATCHES "^(x86_64|amd64)$")
        set(_cef_platform linux64)
        set(_cef_sha1 0b4c2189e3b93033bbef13add1a86f66849c1e62)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND _cpu MATCHES "^(aarch64|arm64)$")
        set(_cef_platform linuxarm64)
        set(_cef_sha1 ea8f06c33514f9f3d75ca003437f9ac45223e07d)
        set(PROJECT_ARCH arm64) # for cef_variables.cmake, Linux reports aarch64
    elseif(WIN32 AND _cpu MATCHES "^(x86_64|amd64)$")
        set(_cef_platform windows64)
        set(_cef_sha1 1fedfc8dd0274caec9183a709ad1ed7856867b91)
    elseif(APPLE AND _cpu STREQUAL "arm64")
        set(_cef_platform macosarm64)
        set(_cef_sha1 6393d8a838d3b29f6b6939b970b5931dde2082aa)
        set(PROJECT_ARCH arm64) # for cef_variables.cmake
    endif()
    if(_cef_platform)
        set(_sdl_with_cef ON)
    else()
        message(WARNING "sdl player: no CEF for ${CMAKE_SYSTEM_NAME} ${_cpu}, built without web pages")
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
    if(WIN32 OR APPLE)
        # without sandbox, like no_sandbox in cef_web_view.cpp (Windows: no bootstrap, macOS: no cef_sandbox)
        set(USE_SANDBOX OFF CACHE BOOL "" FORCE)
    endif()
    # Windows: the wrapper with the DLL runtime (/MD) of the player and vcpkg; the default /MT of CEF does not link
    set(CEF_RUNTIME_LIBRARY_FLAG "/MD" CACHE STRING "" FORCE)
    # cef_variables.cmake empties the C++ flags of CMake for Ninja on Windows: kept, otherwise the player would be built
    # without optimization (/O2)
    foreach(_flags CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE)
        set(_saved_${_flags} "${${_flags}}")
    endforeach()
    find_package(CEF REQUIRED)
    foreach(_flags CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE)
        set(${_flags} "${_saved_${_flags}}")
    endforeach()
    add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} ${CMAKE_CURRENT_BINARY_DIR}/libcef_dll_wrapper)

    target_sources(sdl PRIVATE cef_web_view.cpp)
    target_compile_definitions(sdl PRIVATE PLAYER_SDL_WITH_CEF)
    target_include_directories(sdl SYSTEM PRIVATE ${CEF_ROOT})
    if(APPLE)
        # the framework is loaded at run time (CefScopedLibraryLoader), not linked; the application class of SDL
        # gets the protocol of CEF (Objective-C++)
        enable_language(OBJCXX)
        target_sources(sdl PRIVATE mac/cef_application.mm)
        target_link_libraries(sdl PRIVATE libcef_dll_wrapper ${CEF_STANDARD_LIBS})
    elseif(WIN32)
        target_link_libraries(sdl PRIVATE libcef_dll_wrapper ${CEF_ROOT}/Release/libcef.lib ${CEF_STANDARD_LIBS})
    else()
        target_link_libraries(sdl PRIVATE libcef_dll_wrapper ${CEF_ROOT}/Release/libcef.so)
        set_target_properties(sdl PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN") # libcef.so next to it
    endif()

    if(APPLE)
        # The framework in Contents/Frameworks (its resources and locales inside), next to it the helper apps
        # "garlic-player-sdl Helper", "… Helper (Alerts)", "(GPU)", "(Plugin)", "(Renderer)": CEF starts its
        # subprocesses in them, found by these names.
        COPY_MAC_FRAMEWORK(sdl ${CEF_ROOT}/Release $<TARGET_BUNDLE_DIR:sdl>)
        foreach(_suffixes ${CEF_HELPER_APP_SUFFIXES})
            string(REPLACE ":" ";" _suffixes "${_suffixes}")
            list(GET _suffixes 0 _name_suffix)
            list(GET _suffixes 1 _target_suffix)
            list(GET _suffixes 2 _plist_suffix)
            set(_helper sdl_helper${_target_suffix})
            set(_helper_name "garlic-player-sdl Helper${_name_suffix}")
            add_executable(${_helper} MACOSX_BUNDLE mac/cef_helper.cpp)
            SET_EXECUTABLE_TARGET_PROPERTIES(${_helper})
            target_link_libraries(${_helper} PRIVATE libcef_dll_wrapper ${CEF_STANDARD_LIBS})
            set_target_properties(${_helper} PROPERTIES
                OUTPUT_NAME "${_helper_name}"
                MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/mac/helper-Info.plist.in
                MACOSX_BUNDLE_BUNDLE_NAME "${_helper_name}"
                MACOSX_BUNDLE_GUI_IDENTIFIER "com.garlic-signage.garlic-player-sdl.helper${_plist_suffix}"
            )
            add_dependencies(sdl ${_helper})
            add_custom_command(TARGET sdl POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:${_helper}>"
                        "$<TARGET_BUNDLE_DIR:sdl>/Contents/Frameworks/${_helper_name}.app"
                VERBATIM
            )
        endforeach()
    else()
        # libcef, its resources and locales next to the program, where CEF looks for them
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
    endif()
    message(STATUS "sdl player: web pages by CEF ${CEF_VERSION} (${_cef_platform})")
else()
    target_sources(sdl PRIVATE web_view.cpp) # no web engine: web pages fail, the core goes on
endif()

# The font of the menu and the dialogs where main.cpp looks for it (SDL_GetBasePath())
add_custom_command(TARGET sdl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_sdl_resource_dir}/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/../fonts/DejaVuSans.ttf
            ${_sdl_resource_dir}/fonts/DejaVuSans.ttf
)

option(GARLIC_PLAYER_SDL_TESTS "Build the tests of the SDL player (doctest)" ON)
if(GARLIC_PLAYER_SDL_TESTS)
    add_subdirectory(tests)
endif()
