idf_component_register(SRCS iot_box_task.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES
                       REQUIRES iot_common parameters filesystem iot_can iot_modbus data_record utilities)
