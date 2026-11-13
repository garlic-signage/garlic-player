# The client against a fake supervisor (QLocalServer); the cases of supervisor_protocol/player.jsonl come from the
# description of the protocol, so the player and every supervisor test the same messages.
find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(player_supervisor_tests test_supervisor_client.cpp test_blank_screen.cpp main.cpp)
set_target_properties(player_supervisor_tests PROPERTIES AUTOMOC ON CXX_EXTENSIONS OFF)
target_link_libraries(player_supervisor_tests PRIVATE player_supervisor Qt6::Test)
target_compile_definitions(player_supervisor_tests PRIVATE
    SUPERVISOR_PROTOCOL_CASES="${CMAKE_CURRENT_SOURCE_DIR}/../../../core/docs/supervisor_protocol/player.jsonl")
add_test(NAME player_supervisor COMMAND player_supervisor_tests)
