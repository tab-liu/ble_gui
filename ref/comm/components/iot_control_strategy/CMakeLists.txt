idf_component_register(SRCS off_grid_load_power.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES storm_warning
                       REQUIRES mbedtls security iot_common parameters utilities iot_can)