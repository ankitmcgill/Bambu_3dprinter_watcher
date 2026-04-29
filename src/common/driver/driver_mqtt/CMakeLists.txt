idf_component_register(SRCS "driver_mqtt.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES
                            util_dataqueue
                            defines
                            mqtt
                            esp-tls
                       REQUIRES
                            freertos
)
