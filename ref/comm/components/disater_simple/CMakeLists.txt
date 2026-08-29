idf_component_register(SRCS disater_warn_simple.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                       REQUIRES iot_common parameters utilities esp_hw_support iot_can http_client_ota)