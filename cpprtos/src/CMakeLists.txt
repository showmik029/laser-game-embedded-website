# ---- Paho MQTT Embedded-C (build only the core client + packet code) ----
add_library(paho_embedded_mqtt STATIC
        ${PAHO_EMBEDDED_C_PATH}/MQTTPacket/src/MQTTConnectClient.c
        ${PAHO_EMBEDDED_C_PATH}/MQTTPacket/src/MQTTDeserializePublish.c
        ${PAHO_EMBEDDED_C_PATH}/MQTTPacket/src/MQTTFormat.c
        ${PAHO_EMBEDDED_C_PATH}/MQTTPacket/src/MQTTPacket.c
        ${PAHO_EMBEDDED_C_PATH}/MQTTPacket/src/MQTTSerializePublish.c
        ${PAHO_EMBEDDED_C_PATH}/MQTTPacket/src/MQTTSubscribeClient.c
        ${PAHO_EMBEDDED_C_PATH}/MQTTPacket/src/MQTTUnsubscribeClient.c
        ${PAHO_EMBEDDED_C_PATH}/MQTTClient-C/src/MQTTClient.c
)

target_include_directories(paho_embedded_mqtt PUBLIC
        ${PAHO_EMBEDDED_C_PATH}/MQTTPacket/src
        ${PAHO_EMBEDDED_C_PATH}/MQTTClient-C/src
        ${CMAKE_CURRENT_LIST_DIR}/mqtt
)

target_compile_definitions(paho_embedded_mqtt PRIVATE
        MQTTCLIENT_PLATFORM_HEADER=pico_mqtt_platform.h
)

add_executable(${ProjectName}
    main.cpp

    display/ssd1306_i2c.cpp
    display/ssd1306_i2c.hpp
    display/font5x7.hpp

    input/joystick.cpp
    input/joystick.hpp
    input/debounced_button.hpp

    ui/menu.cpp
    ui/menu.hpp

    wifi/wifi_manager.cpp
    wifi/wifi_manager.hpp
    wifi/wifi_secrets.hpp
    wifi/lwipopts.h

    gamemodes/snake/snake_mode.cpp
    gamemodes/snake/snake_mode.hpp
    gamemodes/mqtt_game1/mqtt_game1_mode.cpp
    gamemodes/mqtt_game1/mqtt_game1_mode.hpp

    laser/laser.cpp
    laser/laser.hpp

    mqtt/pico_mqtt_platform.c
    mqtt/pico_mqtt_platform.h
    mqtt/mqtt_manager.cpp
    mqtt/mqtt_manager.hpp
)

target_include_directories(${ProjectName} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/display
    ${CMAKE_CURRENT_LIST_DIR}/input
    ${CMAKE_CURRENT_LIST_DIR}/ui
    ${CMAKE_CURRENT_LIST_DIR}/wifi
    ${CMAKE_CURRENT_LIST_DIR}/mqtt
)

target_link_libraries(${ProjectName}
    pico_stdlib
    hardware_i2c
    hardware_adc
    FreeRTOS-Kernel-Heap4
    pico_cyw43_arch_lwip_sys_freertos
    paho_embedded_mqtt
)

pico_add_extra_outputs(${ProjectName})

# Disable usb output, enable uart output
pico_enable_stdio_usb(${ProjectName} 0)
pico_enable_stdio_uart(${ProjectName} 1)
