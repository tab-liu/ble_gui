idf_component_register(SRCS smart_tou_ctrl_v1.c ac_ems_cloud.c smart_common.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES
                       REQUIRES filesystem utilities)