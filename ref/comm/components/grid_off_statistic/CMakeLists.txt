idf_component_register(SRCS grid_off_statistic.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES
                       REQUIRES iot_common parameters iot_can http_client_ota)
