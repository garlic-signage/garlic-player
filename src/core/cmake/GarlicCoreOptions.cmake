# Common compile settings for all targets of garlic-core.
#
# - C++23 without compiler extensions
# - no exceptions: errors are returned as std::expected
# - strict warnings
#
# Test executables pass TEST:
# - exceptions are allowed: doctest needs them so that a failing REQUIRE only ends the current test case. The
#   libraries under test are still built without exceptions.
# - Clang: doctest includes <ciso646>, which libstdc++ >= 15 marks deprecated with #warning.

function(garlic_core_set_options target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "TEST" "" "")

    target_compile_features(${target} PUBLIC cxx_std_23)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(arg_TEST)
            target_compile_options(${target} PRIVATE /EHsc)
        else()
            target_compile_options(${target} PRIVATE /EHs-c-)
            target_compile_definitions(${target} PRIVATE _HAS_EXCEPTIONS=0)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        if(NOT arg_TEST)
            target_compile_options(${target} PRIVATE -fno-exceptions)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target} PRIVATE "-Wno-#warnings")
        endif()
    endif()
endfunction()
