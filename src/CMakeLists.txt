set(DRONEACTION_CORE_SOURCES
    AppConfig.cpp
    AppConfig.h
    CommandTypes.cpp
    CommandTypes.h
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
target_link_libraries(droneaction_core PUBLIC Qt6::Core Qt6::Network)

if(DRONEACTION_ENABLE_MAVSDK)
    target_compile_definitions(droneaction_core PUBLIC DRONEACTION_WITH_MAVSDK=1)
    target_link_libraries(droneaction_core PUBLIC MAVSDK::mavsdk)
endif()

qt_add_executable(droneaction
    main.cpp
    MainWindow.cpp
    MainWindow.h
)

target_link_libraries(droneaction PRIVATE droneaction_core Qt6::Widgets)

install(TARGETS droneaction RUNTIME DESTINATION bin)
