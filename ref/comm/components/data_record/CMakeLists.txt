idf_component_register(SRCS dev_data_record.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                       REQUIRES iot_common parameters filesystem iot_can iot_modbus tou_ctrl black_box reg_log dev_access_log
                       energy)
