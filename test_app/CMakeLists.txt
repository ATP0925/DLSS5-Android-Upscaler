cmake_minimum_required(VERSION 3.22.1)
project(VSU_Test_Harness LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(vsu_test_runner
    src/main.cpp
)

target_include_directories(vsu_test_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../shaders
)

target_link_libraries(vsu_test_runner PRIVATE
    vsu_core
    android
    log
    vulkan
)
