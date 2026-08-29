idf_component_register(SRCS modbus_data.c modbus_protocol.c modbus_slave.c modbus_master.c modbus_tcp_slave.c tcp_server.c tcps_server.c modbus_ble_client.c modbus_tlv.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES
                       REQUIRES mbedtls security iot_common parameters utilities iot_can tou_ctrl iot_wifi data_record iot_mqtt iot_xmodem smart_ctrl iot_log iot_cloud_sync)