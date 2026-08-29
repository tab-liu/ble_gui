idf_component_register(SRCS udt_exp_iot.c udt_port.c udt_transfer.c udt_incremental.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                       REQUIRES iot_common parameters iot_can iot_modbus data_record iot_mqtt bluetooth reg_log dev_access_log 
                       energy)
