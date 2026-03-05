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

# Shared "laser_common" (Wi-Fi + MQTT glue + runtime stats)
add_library(laser_common STATIC
        freertos/runtime_stats.cpp

        wifi/wifi_manager.cpp
        wifi/wifi_manager.hpp
        wifi/lwipopts.h
        wifi/wifi_secrets.hpp

        mqtt/pico_mqtt_platform.c
        mqtt/pico_mqtt_platform.h
        mqtt/mqtt_manager.cpp
        mqtt/mqtt_manager.hpp
)

target_include_directories(laser_common PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/wifi
        ${CMAKE_CURRENT_LIST_DIR}/mqtt
        ${CMAKE_CURRENT_LIST_DIR}/freertos
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(laser_common PUBLIC
        pico_stdlib
        FreeRTOS-Kernel-Heap4
        pico_cyw43_arch_lwip_sys_freertos
        paho_embedded_mqtt
)