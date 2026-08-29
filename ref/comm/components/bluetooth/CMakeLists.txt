idf_component_register(SRCS gatts_svr.c drv_nimble.c ble_dev.c ble_client_central.c ble_crypt.c app_bt.c peer.c misc.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES
                       REQUIRES bt iot_common parameters mbedtls utilities iot_can iot_xmodem iot_modbus udt main)