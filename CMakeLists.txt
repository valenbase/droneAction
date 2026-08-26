cmake_minimum_required(VERSION 3.22)

project(DroneAction
    VERSION 0.1.0
    DESCRIPTION "G12 drone monitoring and control AppImage"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

option(DRONEACTION_ENABLE_MAVSDK "Link MAVSDK when it is available" OFF)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Network Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET
    gstreamer-1.0>=1.18
    gstreamer-app-1.0>=1.18
    gstreamer-video-1.0>=1.18
)

if(DRONEACTION_ENABLE_MAVSDK)
    find_package(MAVSDK REQUIRED)
endif()

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)

install(FILES packaging/com.valenbase.DroneAction.desktop
        DESTINATION share/applications)
install(FILES packaging/com.valenbase.DroneAction.svg
        DESTINATION share/icons/hicolor/scalable/apps)
