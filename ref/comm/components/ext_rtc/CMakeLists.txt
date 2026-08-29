idf_component_register(SRCS drv_pcf8563.c ext_rtc.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                       REQUIRES iot_common filesystem parameters utilities iic_bus)