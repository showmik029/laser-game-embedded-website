add_executable(laser_target
        main.cpp
        target/target_controller.cpp
        target/target_controller.hpp
)

target_include_directories(laser_target PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/target
)

target_link_libraries(laser_target
        laser_common
        hardware_adc
)

pico_add_extra_outputs(laser_target)

pico_enable_stdio_usb(laser_target 0)
pico_enable_stdio_uart(laser_target 1)