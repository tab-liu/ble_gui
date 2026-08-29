idf_component_register(SRCS drv_gd25q128.c ext_flash.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                       REQUIRES spi_flash iot_common filesystem parameters utilities iic_bus)