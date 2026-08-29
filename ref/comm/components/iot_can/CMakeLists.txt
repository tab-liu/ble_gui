set(srcs ./can_data/can_data.c
        ./can_data/data_summary.c
        ./can_protocol/can_protocol.c
        ./can_protocol/can_transmit.c
        ./can_ota/can_ota_receiver.c
        ./can_ota/can_ota_sender.c
        ./can_ota/iot_ota.c
        ./bms_can_alpha/bms_can.c
        ./bms_can_alpha/bms_ota.c
        ./can_product/can_product.c)

list(TRANSFORM srcs PREPEND ${CMAKE_CURRENT_LIST_DIR}/)

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "." "./can_data/" "./can_protocol/" "./can_ota/" "./bms_can_alpha/" "./can_product/"
                       PRIV_REQUIRES iot_log
                       REQUIRES iot_common parameters driver iot_modbus utilities app_update udt sync_time storm_warning image_handle tou_ctrl iot_xmodem low_power reg_log dev_access_log)