idf_component_register(SRCS tou_relay_ctrl.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                       REQUIRES iot_common esp_timer parameters iot_can)
