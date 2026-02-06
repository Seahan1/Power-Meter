cmake_minimum_required(VERSION 3.16)
project(ProPowerMonitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort)

# 源文件列表
set(SOURCES
    main.cpp
    mainwindow.h
    mainwindow.cpp
    oscilloscope.h
    oscilloscope.cpp
)

add_executable(ProPowerMonitor ${SOURCES})

target_link_libraries(ProPowerMonitor PRIVATE 
    Qt6::Widgets 
    Qt6::SerialPort
)

# 确保在 Windows 上作为 GUI 程序运行（不显示控制台）
if(WIN32)
    set_target_properties(ProPowerMonitor PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
