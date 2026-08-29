idf_component_register(SRCS json_cmd.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                       REQUIRES iot_common nvs_store filesystem esp_wifi json parameters)