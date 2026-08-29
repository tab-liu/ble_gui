idf_component_register(SRCS ota_common.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES iot_xmodem http_client_ota
                       REQUIRES esp_event iot_can utilities dev_common)