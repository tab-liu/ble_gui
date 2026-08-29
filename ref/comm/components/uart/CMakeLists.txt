idf_component_register(SRCS uart_device_process.c rs485_transmit.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES
                       REQUIRES iot_common parameters soc hal nvs_store utilities)
