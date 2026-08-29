idf_component_register(SRCS log_redirect.c circular_queue.c aiot_log.c aiot_can_log.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES wlcc utilities filesystem
                       REQUIRES esp_wifi nvs_flash)