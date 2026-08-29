idf_component_register(SRCS sync_time.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                       REQUIRES iot_common parameters utilities ext_rtc iot_can)