idf_component_register(SRCS uart_ota.c xmodem_client.c xmodem_transmitter.c xmodem.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                       REQUIRES iot_common nvs_store filesystem iot_modbus utilities iot_can image_handle file_ota)