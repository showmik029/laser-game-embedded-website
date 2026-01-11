add_executable(${ProjectName}
    main.cpp
    display/ssd1306_i2c.cpp
    input/joystick.cpp
    ui/menu.cpp
)

target_include_directories(${ProjectName} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/display
    ${CMAKE_CURRENT_LIST_DIR}/input
    ${CMAKE_CURRENT_LIST_DIR}/ui
)

target_link_libraries(${ProjectName}
    pico_stdlib
    hardware_i2c
    hardware_adc
    FreeRTOS-Kernel-Heap4
)

pico_add_extra_outputs(${ProjectName})

# Disable usb output, enable uart output
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)
