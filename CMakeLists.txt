cmake_minimum_required(VERSION 3.22.1)
project(DLSS5_Android_Vulkan_Upscaler CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

add_library(VkLayer_dlss5 SHARED
    src/dlss_layer.cpp
    src/config_manager.cpp
)

target_include_directories(VkLayer_dlss5 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(VkLayer_dlss5 PRIVATE
    android
    log
    ${Vulkan_LIBRARIES}
)

set_target_properties(VkLayer_dlss5 PROPERTIES
    OUTPUT_NAME "VkLayer_dlss5"
    PREFIX "lib"
    SUFFIX ".so"
)
