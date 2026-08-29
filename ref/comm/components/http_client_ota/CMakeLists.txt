idf_component_register(SRCS http_client.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                       REQUIRES esp_http_client iot_common bootloader_support json json_cmd parameters iot_can webserver app_update esp_partition esp_app_format image_handle mbedtls iot_xmodem iot_mqtt file_ota grid_off_statistic)
