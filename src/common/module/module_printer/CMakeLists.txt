idf_component_register(SRCS "module_printer.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES
                            module_mqtt
                            driver_nvs
                            util_dataqueue
                            defines
                            json
                       REQUIRES
                            esp_timer
                            freertos
)
