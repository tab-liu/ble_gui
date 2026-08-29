idf_component_register(SRCS wlcc_interface.c wlcc_process.c wlcc_protocol.c wlcc_crypt.c wlcc_tlv.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                       REQUIRES  iot_common parameters mbedtls esp_hw_support dev_info)
