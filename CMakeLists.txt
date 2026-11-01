cmake_minimum_required(VERSION 3.22.1)
project(VulkanSpatialUpscaler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 1. Locate glslc shader compiler
find_program(GLSLC_EXECUTABLE glslc
    HINTS
        ${ANDROID_NDK}/shader-tools/${ANDROID_HOST_TAG}
        $ENV{ANDROID_NDK_HOME}/shader-tools/${ANDROID_HOST_TAG}
        $ENV{ANDROID_NDK}/shader-tools/linux-x86_64
        $ENV{VULKAN_SDK}/bin
)

if(GLSLC_EXECUTABLE)
    message(STATUS "[VSU] Found glslc compiler at: ${GLSLC_EXECUTABLE}")

    set(SHADER_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr_easu.comp
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/cas.comp
    )

    foreach(SHADER_SRC ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER_SRC} NAME)
        set(SPV_OUT ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER_NAME}.spv)
        
        add_custom_command(
            OUTPUT ${SPV_OUT}
            COMMAND ${GLSLC_EXECUTABLE} -O --target-env=vulkan1.1 ${SHADER_SRC} -o ${SPV_OUT}
            DEPENDS ${SHADER_SRC}
            COMMENT "[VSU] Compiling GLSL shader to SPIR-V: ${SHADER_NAME}"
        )
        list(APPEND SPV_TARGETS ${SPV_OUT})
    endforeach()

    add_custom_target(CompileShaders ALL DEPENDS ${SPV_TARGETS})
else()
    message(WARNING "[VSU] glslc compiler not found in host PATH; utilizing embedded precompiled C++ SPIR-V headers.")
endif()

# 2. Build Core Spatial Upscaler Engine (Standalone, Unit-Testable Library)
add_library(vsu_core STATIC
    src/core/vsu_engine.cpp
)

target_include_directories(vsu_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
)

target_link_libraries(vsu_core PUBLIC
    android
    log
    vulkan
)

# 3. Build Vulkan Layer Shared Object (libVkLayer_vsu.so)
add_library(VkLayer_vsu SHARED
    src/dlss_layer.cpp
    src/config_manager.cpp
    src/thermal_guard.cpp
    src/anti_cheat_guard.cpp
    src/crash_guard.cpp
)

target_include_directories(VkLayer_vsu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
)

target_link_libraries(VkLayer_vsu PRIVATE
    vsu_core
    android
    log
    vulkan
)

set_target_properties(VkLayer_vsu PROPERTIES
    OUTPUT_NAME "VkLayer_vsu"
    PREFIX "lib"
    SUFFIX ".so"
)

# Also maintain backward-compatible legacy layer alias libVkLayer_dlss5.so
add_library(VkLayer_dlss5 SHARED
    src/dlss_layer.cpp
    src/config_manager.cpp
    src/thermal_guard.cpp
    src/anti_cheat_guard.cpp
    src/crash_guard.cpp
)

target_include_directories(VkLayer_dlss5 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
)

target_link_libraries(VkLayer_dlss5 PRIVATE
    vsu_core
    android
    log
    vulkan
)

set_target_properties(VkLayer_dlss5 PROPERTIES
    OUTPUT_NAME "VkLayer_dlss5"
    PREFIX "lib"
    SUFFIX ".so"
)

# 4. Standalone Test Harness (Verification App)
add_subdirectory(test_app)
