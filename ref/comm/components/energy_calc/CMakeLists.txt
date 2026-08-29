idf_component_register(SRCS energy_record_calc.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES
                       REQUIRES 
                       filesystem 
                       utilities
                       general_ops)