idf_component_register(SRCS "module_mqtt.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES
                            driver_mqtt
                            util_dataqueue
                            defines
                       REQUIRES
                            esp_timer
                            freertos
)
