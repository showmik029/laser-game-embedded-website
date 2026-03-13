add_executable(laser_controller
        main.cpp

        display/ssd1306_i2c.cpp
        display/ssd1306_i2c.hpp
        display/font5x7.hpp

        input/joystick.cpp
        input/joystick.hpp
        input/debounced_button.hpp

        ui/menu.cpp
        ui/menu.hpp

        gamemodes/snake/snake_mode.cpp
        gamemodes/snake/snake_mode.hpp
        gamemodes/mqtt_target/remote_target_mode.cpp
        gamemodes/mqtt_target/remote_target_mode.hpp

        laser/laser.cpp
        laser/laser.hpp
)

target_include_directories(laser_controller PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/display
        ${CMAKE_CURRENT_LIST_DIR}/input
        ${CMAKE_CURRENT_LIST_DIR}/ui
)

target_link_libraries(laser_controller
        laser_common
        hardware_i2c
        hardware_adc
)

pico_add_extra_outputs(laser_controller)

pico_enable_stdio_usb(laser_controller 0)
pico_enable_stdio_uart(laser_controller 1)