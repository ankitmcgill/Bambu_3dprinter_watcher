idf_component_register(SRCS "driver_nvs.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES
                            defines
                       REQUIRES
                            nvs_flash
)
