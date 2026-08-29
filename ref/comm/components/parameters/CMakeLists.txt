idf_component_register(SRCS parameter.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                       REQUIRES iot_common nvs_store filesystem esp_wifi utilities iot_can bt_adv)