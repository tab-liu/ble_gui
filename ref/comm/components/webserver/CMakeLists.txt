idf_component_register(SRCS webserver.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                       REQUIRES  iot_common parameters iot_can iot_modbus esp_http_server iot_can json_cmd mbedtls security http_client_ota
                       EMBED_FILES ./webserver.html)
