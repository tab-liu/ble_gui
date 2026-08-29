idf_component_register(SRCS filesystem.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES vfs joltwallet__littlefs
                       REQUIRES nvs_flash iot_common utilities)
