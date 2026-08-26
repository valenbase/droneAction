add_executable(command_parser_tests test_command_parser.cpp)
target_link_libraries(command_parser_tests PRIVATE droneaction_core Qt6::Test)
add_test(NAME command_parser_tests COMMAND command_parser_tests)

add_executable(app_config_tests test_app_config.cpp)
target_link_libraries(app_config_tests PRIVATE droneaction_core Qt6::Test)
add_test(NAME app_config_tests COMMAND app_config_tests)

add_executable(safety_gate_tests test_safety_gate.cpp)
target_link_libraries(safety_gate_tests PRIVATE droneaction_core Qt6::Test)
add_test(NAME safety_gate_tests COMMAND safety_gate_tests)

add_executable(ardupilot_control_tests test_ardupilot_control.cpp)
target_link_libraries(ardupilot_control_tests PRIVATE droneaction_core Qt6::Test)
add_test(NAME ardupilot_control_tests COMMAND ardupilot_control_tests)

if(DRONEACTION_ENABLE_MAVSDK)
    add_executable(apm_integration_probe apm_integration_probe.cpp)
    target_link_libraries(apm_integration_probe PRIVATE droneaction_core)
endif()

add_executable(video_service_probe video_service_probe.cpp)
target_link_libraries(video_service_probe PRIVATE droneaction_core)
