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
        BUILD_RPATH "@executable_path/../Frameworks" # libmpv
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
    set(_sdl_resource_dir $<TARGET_BUNDLE_CONTENT_DIR:sdl>/Resources)
else()
    set(_sdl_resource_dir $<TARGET_FILE_DIR:sdl>)
endif()

# libmpv for video and audio (render API with OpenGL; client API 2.0 = mpv 0.35, Debian Trixie has 0.40). From
# pkg-config (Linux), or from MPV_DIR if set: Windows the libmpv package mpv-dev-x86_64-… of shinchiro, unpacked
# (include/mpv/*.h, libmpv-2.dll, libmpv.dll.a, mpv.def); macOS the prefix of an own build (include/mpv/*.h,
# lib/libmpv.dylib), copied into Contents/Frameworks of the bundle.
option(PLAYER_SDL_WITH_MPV "Video and audio by libmpv, if found" ON)
set(MPV_DIR "" CACHE PATH "Folder of libmpv instead of pkg-config (Windows: mpv-dev-x86_64-… of shinchiro, macOS: prefix)")
if(PLAYER_SDL_WITH_MPV)
    if(NOT MPV_DIR)
        find_package(PkgConfig)
    endif()
    if(PkgConfig_FOUND AND NOT MPV_DIR)
        pkg_check_modules(MPV IMPORTED_TARGET mpv>=2.0)
    endif()
    if(MPV_FOUND)
        target_sources(sdl PRIVATE mpv_media.cpp)
        target_compile_definitions(sdl PRIVATE PLAYER_SDL_WITH_MPV)
        target_link_libraries(sdl PRIVATE PkgConfig::MPV)
        message(STATUS "sdl player: video and audio by libmpv ${MPV_VERSION}")
    else()
        find_path(MPV_INCLUDE_DIR mpv/render_gl.h HINTS ${MPV_DIR} PATH_SUFFIXES include)
        file(GLOB _mpv_dlls ${MPV_DIR}/libmpv*.dll ${MPV_DIR}/bin/libmpv*.dll)
        if(MSVC AND _mpv_dlls AND EXISTS ${MPV_DIR}/mpv.def)
            # MSVC: the import library from mpv.def (lib.exe), for the DLL of the package
            list(GET _mpv_dlls 0 _mpv_dll)
            get_filename_component(_mpv_dll_name ${_mpv_dll} NAME)
            set(MPV_LIBRARY ${CMAKE_CURRENT_BINARY_DIR}/mpv.lib)
            execute_process(COMMAND ${CMAKE_AR} /nologo /def:${MPV_DIR}/mpv.def /name:${_mpv_dll_name} /machine:x64
                                    /out:${MPV_LIBRARY}
                            RESULT_VARIABLE _mpv_lib_result)
            if(NOT _mpv_lib_result EQUAL 0)
                unset(MPV_LIBRARY)
            endif()
        else()
            find_library(MPV_LIBRARY NAMES mpv libmpv libmpv.dll.a HINTS ${MPV_DIR} PATH_SUFFIXES lib)
        endif()
        if(MPV_INCLUDE_DIR AND MPV_LIBRARY)
            target_sources(sdl PRIVATE mpv_media.cpp)
            target_compile_definitions(sdl PRIVATE PLAYER_SDL_WITH_MPV)
            target_include_directories(sdl SYSTEM PRIVATE ${MPV_INCLUDE_DIR})
            target_link_libraries(sdl PRIVATE ${MPV_LIBRARY})
            foreach(_dll ${_mpv_dlls}) # next to the program
                add_custom_command(TARGET sdl POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_dll}
                                   $<TARGET_FILE_DIR:sdl>)
            endforeach()
            if(APPLE AND MPV_LIBRARY MATCHES "\\.dylib$")
                # into Contents/Frameworks, found by the program through @rpath instead of the path of the build;
                # signed again ad hoc, install_name_tool breaks the signature (arm64 starts nothing unsigned)
                get_filename_component(_mpv_real ${MPV_LIBRARY} REALPATH)
                execute_process(COMMAND otool -D ${_mpv_real} OUTPUT_VARIABLE _mpv_id OUTPUT_STRIP_TRAILING_WHITESPACE)
                string(REGEX REPLACE "^[^\n]*\n" "" _mpv_id "${_mpv_id}") # first line: the file
                get_filename_component(_mpv_name "${_mpv_id}" NAME)
                set(_mpv_frameworks $<TARGET_BUNDLE_CONTENT_DIR:sdl>/Frameworks)
                add_custom_command(TARGET sdl POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory ${_mpv_frameworks}
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_mpv_real} ${_mpv_frameworks}/${_mpv_name}
                    COMMAND chmod u+w ${_mpv_frameworks}/${_mpv_name}
                    COMMAND install_name_tool -id @rpath/${_mpv_name} ${_mpv_frameworks}/${_mpv_name}
                    COMMAND install_name_tool -change ${_mpv_id} @rpath/${_mpv_name} $<TARGET_FILE:sdl>
                    COMMAND codesign --force --sign - ${_mpv_frameworks}/${_mpv_name}
                    COMMAND codesign --force --sign - $<TARGET_FILE:sdl>
                    VERBATIM
                )
            endif()
            message(STATUS "sdl player: video and audio by libmpv from ${MPV_DIR}")
        else()
            message(WARNING "sdl player: libmpv not found (libmpv-dev, mpv; MPV_DIR), built without video and audio")
        endif()
    endif()
endif()

# CEF for web pages, off-screen rendered into textures: Linux (x86_64, aarch64), Windows (x64), macOS (arm64). The minimal binary distribution is downloaded (about 350 MB)
# and its C++ wrapper built.
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
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/../qwidget/fonts/DejaVuSans.ttf
            ${_sdl_resource_dir}/fonts/DejaVuSans.ttf
)

option(GARLIC_PLAYER_SDL_TESTS "Build the tests of the SDL player (doctest)" ON)
if(GARLIC_PLAYER_SDL_TESTS)
    add_subdirectory(tests)
endif()
