idf_component_register(SRCS "module_printer.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES
                            module_mqtt
                            util_dataqueue
                            defines
                       REQUIRES
                            esp_timer
                            freertos
)
