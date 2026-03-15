cmake_minimum_required(VERSION 3.12)

set(PICO_BOARD pico_w CACHE STRING "Board type")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)

set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS-KernelV10.6.2)
set(PAHO_EMBEDDED_C_PATH ${CMAKE_CURRENT_SOURCE_DIR}/paho.mqtt.embedded-c)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

set(FREERTOS_CONFIG_FILE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(laser_labs_multi C CXX ASM)
pico_sdk_init()

add_subdirectory(common)
add_subdirectory(apps/controller)
add_subdirectory(apps/target)