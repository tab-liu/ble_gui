idf_component_register(SRCS iot_rsa.c iot_nvs_encrypt.c iot_partition.c md5.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                       REQUIRES iot_common utilities mbedtls esp_hw_support esp_partition app_update iot_can bluetooth)