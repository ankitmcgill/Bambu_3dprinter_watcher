idf_component_register(SRCS "util_dataqueue.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES
                            defines
                       REQUIRES
                            freertos
)
