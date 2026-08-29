idf_component_register(SRCS general_conversions.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES
                       REQUIRES mbedtls security iot_common parameters utilities iot_can tou_ctrl data_record )