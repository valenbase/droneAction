add_executable(command_parser_tests test_command_parser.cpp)
target_link_libraries(command_parser_tests PRIVATE droneaction_core Qt6::Test)
add_test(NAME command_parser_tests COMMAND command_parser_tests)

add_executable(app_config_tests test_app_config.cpp)
target_link_libraries(app_config_tests PRIVATE droneaction_core Qt6::Test)
add_test(NAME app_config_tests COMMAND app_config_tests)

add_executable(control_watchdog_tests test_control_watchdog.cpp)
target_link_libraries(control_watchdog_tests PRIVATE droneaction_core Qt6::Test)
add_test(NAME control_watchdog_tests COMMAND control_watchdog_tests)

add_executable(settings_dialog_tests test_settings_dialog.cpp ../src/SettingsDialog.cpp ../src/SettingsDialog.h)
target_link_libraries(settings_dialog_tests PRIVATE droneaction_core Qt6::Widgets Qt6::Test)
add_test(NAME settings_dialog_tests COMMAND settings_dialog_tests)
set_tests_properties(settings_dialog_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME command_stream_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_command_stream.py)
    set_tests_properties(command_stream_tests PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1" TIMEOUT 10)
endif()

add_executable(udp_command_order_tests test_udp_command_order.cpp)
target_link_libraries(udp_command_order_tests PRIVATE droneaction_core Qt6::Test)
add_test(NAME udp_command_order_tests COMMAND udp_command_order_tests)

add_executable(safety_gate_tests test_safety_gate.cpp)
target_link_libraries(safety_gate_tests PRIVATE droneaction_core Qt6::Test)
add_test(NAME safety_gate_tests COMMAND safety_gate_tests)

add_executable(ardupilot_control_tests test_ardupilot_control.cpp)
target_link_libraries(ardupilot_control_tests PRIVATE droneaction_core Qt6::Test)
add_test(NAME ardupilot_control_tests COMMAND ardupilot_control_tests)

if(DRONEACTION_ENABLE_MAVSDK)
    add_executable(px4_control_integration_tests test_px4_control_integration.cpp)
    target_link_libraries(px4_control_integration_tests PRIVATE droneaction_core Qt6::Test)
    add_test(NAME px4_control_integration_tests COMMAND px4_control_integration_tests)
    set_tests_properties(px4_control_integration_tests PROPERTIES TIMEOUT 90)
    add_executable(px4_sitl_probe px4_sitl_probe.cpp)
    target_link_libraries(px4_sitl_probe PRIVATE droneaction_core)
    add_executable(apm_integration_probe apm_integration_probe.cpp)
    target_link_libraries(apm_integration_probe PRIVATE droneaction_core)
endif()

add_executable(video_service_probe video_service_probe.cpp)
target_link_libraries(video_service_probe PRIVATE droneaction_core)
