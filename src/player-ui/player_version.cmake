# The version of the player software in its user agent, from the file VERSION next to this one. The build server
# writes its version there (echo "2.0.0-beta.$BUILD" > src/player-ui/VERSION); in the repository it is the default.
# garlic-hub gives the full index template (multizone) from build 566 on: the number after the last dot.
file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/VERSION GARLIC_PLAYER_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/VERSION)

function(garlic_player_version target)
    target_compile_definitions(${target} PRIVATE PLAYER_VERSION="${GARLIC_PLAYER_VERSION}")
endfunction()
