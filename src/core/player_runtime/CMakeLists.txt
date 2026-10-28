add_library(garlic_core_player_runtime STATIC
    player_runtime_api.cpp
    reboot_schedule.cpp
    threaded_runtime.cpp
)
add_library(garlic::core::player_runtime ALIAS garlic_core_player_runtime)
garlic_core_set_options(garlic_core_player_runtime)

# Includes are written as "player_runtime/player_runtime_api.hpp"
target_include_directories(garlic_core_player_runtime PUBLIC ${PROJECT_SOURCE_DIR})

# ThreadedRuntime: std::thread
find_package(Threads REQUIRED)
target_link_libraries(garlic_core_player_runtime PUBLIC
    garlic::core::content garlic::core::reports garlic::core::tasks garlic::core::rest_api garlic::core::httpd
    Threads::Threads
)

# HTTP server (libmicrohttpd) and system information only where they are built (not on WASM and ESP32)
target_compile_definitions(garlic_core_player_runtime PRIVATE
    GARLIC_CORE_WITH_HTTPD=$<BOOL:${GARLIC_CORE_WITH_HTTPD}>
    GARLIC_CORE_WITH_SYSTEM=$<TARGET_EXISTS:garlic_core_system>
)
if(TARGET garlic_core_system)
    target_link_libraries(garlic_core_player_runtime PUBLIC garlic::core::system)
endif()
