idf_component_register(SRCS image_handle.c image_back.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                       REQUIRES iot_common bootloader_support parameters app_update esp_partition iot_can)
