# Common compile settings for all targets of garlic-core.
#
# - C++23 without compiler extensions
# - no exceptions: errors are returned as std::expected
# - strict warnings

function(garlic_core_set_options target)
    target_compile_features(${target} PUBLIC cxx_std_23)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHs-c-)
        target_compile_definitions(${target} PRIVATE _HAS_EXCEPTIONS=0)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow
            -fno-exceptions
        )
    endif()
endfunction()
