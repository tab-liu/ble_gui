idf_component_register(SRCS iot_sleep.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                       REQUIRES iot_common parameters driver esp_hw_support esp_pm iot_can iot_modbus wlcc usb_host)