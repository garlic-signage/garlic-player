# Only on the native platforms; on WASM and ESP32 the host fills reports::SystemInfo itself.
if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_garlic_core_system_platform platform_linux.cpp)
elseif(WIN32)
    set(_garlic_core_system_platform platform_windows.cpp)
elseif(APPLE)
    set(_garlic_core_system_platform platform_apple.cpp)
else()
    set(_garlic_core_system_platform platform_none.cpp)
endif()

add_library(garlic_core_system STATIC
    system_info.cpp
    ${_garlic_core_system_platform}
)
add_library(garlic::core::system ALIAS garlic_core_system)
garlic_core_set_options(garlic_core_system)

# Includes are written as "system/system_info.hpp"
target_include_directories(garlic_core_system PUBLIC ${PROJECT_SOURCE_DIR})

target_link_libraries(garlic_core_system PUBLIC garlic::core::reports)
if(WIN32)
    target_link_libraries(garlic_core_system PRIVATE iphlpapi psapi ws2_32 icu)
elseif(APPLE)
    target_link_libraries(garlic_core_system PRIVATE "-framework CoreFoundation")
endif()
