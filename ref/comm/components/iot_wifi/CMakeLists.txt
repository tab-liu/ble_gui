idf_component_register(SRCS iot_wifi.c
                       INCLUDE_DIRS "."
                       REQUIRES
                       PRIV_REQUIRES iot_common filesystem esp_wifi parameters iot_mqtt esp_timer wlcc low_power)