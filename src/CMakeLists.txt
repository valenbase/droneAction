set(DRONEACTION_CORE_SOURCES
    ArduPilotControl.cpp
    ArduPilotControl.h
    AppConfig.cpp
    AppConfig.h
    CommandTypes.cpp
    CommandTypes.h
    ControlWatchdog.cpp
    ControlWatchdog.h
    SafetyGate.cpp
    SafetyGate.h
    TelemetryService.cpp
    TelemetryService.h
    UdpCommandServer.cpp
    UdpCommandServer.h
    VideoService.cpp
    VideoService.h
)

add_library(droneaction_core STATIC ${DRONEACTION_CORE_SOURCES})
target_include_directories(droneaction_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(droneaction_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    PkgConfig::GSTREAMER
)

if(DRONEACTION_ENABLE_MAVSDK)
    target_compile_definitions(droneaction_core PUBLIC DRONEACTION_WITH_MAVSDK=1)
    target_link_libraries(droneaction_core PUBLIC MAVSDK::mavsdk)
endif()

qt_add_executable(droneaction
    main.cpp
    InstrumentWidgets.cpp
    InstrumentWidgets.h
    MainWindow.cpp
    MainWindow.h
    MapWidget.cpp
    MapWidget.h
    RunModeDialog.cpp
    RunModeDialog.h
    SettingsDialog.cpp
    SettingsDialog.h
    VideoFrameWidget.cpp
    VideoFrameWidget.h
)

target_link_libraries(droneaction PRIVATE droneaction_core Qt6::Widgets)

install(TARGETS droneaction RUNTIME DESTINATION bin)
