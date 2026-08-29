idf_component_register(SRCS dev_modbus_manage.c dev_modbus_meter_data_handler.c dev_modbus_s1_data_handler.c dev_modbus_other_inv_data_handler.c 
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES
                       REQUIRES mbedtls security iot_common parameters utilities iot_can tou_ctrl iot_wifi data_record iot_mqtt iot_xmodem iot_modbus wlcc)