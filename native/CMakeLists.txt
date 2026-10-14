cmake_minimum_required(VERSION 3.24)
project(eve_viola_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_executable(eve_viola_core_tests
    tests/core_tests.cpp
    src/core.cpp
)
target_include_directories(eve_viola_core_tests PRIVATE src include)
add_test(NAME eve_viola_core_tests COMMAND eve_viola_core_tests)

add_library(eve_viola_native SHARED
    src/eve_viola_native.cpp
    src/eve_window_identity.cpp
    src/window_enumeration.cpp
    src/window_thumbnail.cpp
    src/window_activation.cpp
    src/window_capture_pixels.cpp
    src/window_capture_frame.cpp
    src/native_platform.h
    src/native_status.h
    src/capture_timer.h
    src/eve_window_identity.h
    src/core.cpp
)
target_compile_definitions(eve_viola_native PRIVATE EVE_VIOLA_NATIVE_EXPORTS UNICODE _UNICODE)
target_include_directories(eve_viola_native
    PRIVATE src
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(eve_viola_native PRIVATE dwmapi user32 gdi32)

add_executable(eve_viola_capture_tests tests/capture_tests.cpp)
target_link_libraries(eve_viola_capture_tests PRIVATE eve_viola_native user32 gdi32 dwmapi)
add_test(NAME eve_viola_capture_tests COMMAND eve_viola_capture_tests)
set_tests_properties(eve_viola_capture_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:eve_viola_native>"
    TIMEOUT 60
)
if(EVE_VIOLA_BUILD_QT_APP)
    set(EVE_VIOLA_NATIVE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/cpp")
else()
    set(EVE_VIOLA_NATIVE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/native")
endif()
set_target_properties(eve_viola_native PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${EVE_VIOLA_NATIVE_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${EVE_VIOLA_NATIVE_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${EVE_VIOLA_NATIVE_OUTPUT_DIR}"
)
