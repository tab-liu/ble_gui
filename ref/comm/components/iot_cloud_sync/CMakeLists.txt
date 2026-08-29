idf_component_register(SRCS cloud_sync.c
                       INCLUDE_DIRS "."
                       REQUIRES iot_mqtt iot_common parameters utilities iot_can)
