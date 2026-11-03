# js: the browser frontend of the garlic-player on the core (WASM). The core runs as WebAssembly in the page, the
# player itself is JavaScript (web/). Only with Emscripten; on its own with the presets here, or from the repository
# build with the Emscripten toolchain.
cmake_minimum_required(VERSION 3.25)

project(js VERSION 2.0.0 LANGUAGES CXX)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "the js player is only built with Emscripten (cmake --preset wasm-release)")
endif()

# on its own: the core from the repository
if(NOT TARGET garlic::core::player_runtime)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../core ${CMAKE_CURRENT_BINARY_DIR}/core)
endif()

add_executable(js bindings.cpp)
set_target_properties(js PROPERTIES OUTPUT_NAME garlic-core CXX_EXTENSIONS OFF SUFFIX ".js")
target_compile_features(js PRIVATE cxx_std_23)
target_compile_options(js PRIVATE -Wall -Wextra -Wpedantic -fno-exceptions)
include(${CMAKE_CURRENT_SOURCE_DIR}/../player_version.cmake)
garlic_player_version(js)
target_link_libraries(js PRIVATE garlic::core::player_runtime)

# garlic-core.js as ES module (createGarlicCore); FS and IDBFS: the cache in IndexedDB
target_link_options(js PRIVATE
    -lembind
    -lidbfs.js
    -sMODULARIZE
    -sEXPORT_ES6
    -sEXPORT_NAME=createGarlicCore
    -sENVIRONMENT=web
    -sALLOW_MEMORY_GROWTH
    -sEXPORTED_RUNTIME_METHODS=FS,IDBFS
    -sFORCE_FILESYSTEM
)

# the page next to garlic-core.js and garlic-core.wasm, copied again when a file changed
set(web_files index.html player.css player.js stage.js dialogs.js storage.js sw.js)
set(web_outputs)
foreach(file IN LISTS web_files)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${file}
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/web/${file} ${CMAKE_CURRENT_BINARY_DIR}/${file}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/${file})
    list(APPEND web_outputs ${CMAKE_CURRENT_BINARY_DIR}/${file})
endforeach()
add_custom_target(js_web ALL DEPENDS ${web_outputs})
