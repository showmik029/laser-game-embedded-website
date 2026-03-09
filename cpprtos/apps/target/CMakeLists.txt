add_executable(laser_target_pico1
        main.cpp
        target/target_controller.cpp
        target/target_controller.hpp
)

target_include_directories(laser_target_pico1 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/target
)

target_compile_definitions(laser_target_pico1 PRIVATE
        TARGET_BOARD_ID=\"pico-1\"
        TARGET_MQTT_CLIENT_ID=\"laser-target-pico-1\"
)

target_link_libraries(laser_target_pico1
        laser_common
        hardware_adc
)

pico_add_extra_outputs(laser_target_pico1)

pico_enable_stdio_usb(laser_target_pico1 0)
pico_enable_stdio_uart(laser_target_pico1 1)


add_executable(laser_target_pico2
        main.cpp
        target/target_controller.cpp
        target/target_controller.hpp
)

target_include_directories(laser_target_pico2 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/target
)

target_compile_definitions(laser_target_pico2 PRIVATE
        TARGET_BOARD_ID=\"pico-2\"
        TARGET_MQTT_CLIENT_ID=\"laser-target-pico-2\"
)

target_link_libraries(laser_target_pico2
        laser_common
        hardware_adc
)

pico_add_extra_outputs(laser_target_pico2)

pico_enable_stdio_usb(laser_target_pico2 0)
pico_enable_stdio_uart(laser_target_pico2 1)