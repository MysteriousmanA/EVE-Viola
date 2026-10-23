cmake_minimum_required(VERSION 3.24)
project(eve_viola VERSION 1.1.0 LANGUAGES CXX)

include(CTest)

option(EVE_VIOLA_BUILD_QT_APP "Build the C++ Qt Widgets application" OFF)

add_subdirectory(native)

if(EVE_VIOLA_BUILD_QT_APP)
    find_package(Qt6 REQUIRED COMPONENTS Concurrent Core Gui Multimedia Network Widgets Test)
    add_subdirectory(cpp)
endif()
