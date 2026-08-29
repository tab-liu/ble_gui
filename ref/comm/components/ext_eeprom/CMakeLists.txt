idf_component_register(SRCS drv_24cxx.c eeprom_init.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                       REQUIRES iot_common filesystem parameters utilities iic_bus)