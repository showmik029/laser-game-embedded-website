add_executable(${ProjectName}
    main.cpp

    display/ssd1306_i2c.cpp
    display/ssd1306_i2c.hpp
    display/font5x7.hpp

    input/joystick.cpp
    input/joystick.hpp

    ui/menu.cpp
    ui/menu.hpp

    wifi/wifi_manager.cpp
    wifi/wifi_manager.hpp
    wifi/wifi_secrets.hpp
    wifi/lwipopts.h

    gamemodes/snake/snake_mode.cpp
    gamemodes/snake/snake_mode.hpp
)

target_include_directories(${ProjectName} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/display
    ${CMAKE_CURRENT_LIST_DIR}/input
    ${CMAKE_CURRENT_LIST_DIR}/ui
    ${CMAKE_CURRENT_LIST_DIR}/wifi
)

target_link_libraries(${ProjectName}
    pico_stdlib
    hardware_i2c
    hardware_adc
    FreeRTOS-Kernel-Heap4
    pico_cyw43_arch_lwip_threadsafe_background
)

pico_add_extra_outputs(${ProjectName})

# Disable usb output, enable uart output
pico_enable_stdio_usb(${ProjectName} 0)
pico_enable_stdio_uart(${ProjectName} 1)
