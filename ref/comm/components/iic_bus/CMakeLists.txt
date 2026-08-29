idf_component_register(SRCS i2c_bus.c twi_master.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                       REQUIRES iot_common filesystem utilities)