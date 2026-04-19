idf_component_register(SRCS "driver_wifi.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES
                            driver_chipinfo
                            util_dataqueue
                            defines
                       REQUIRES
                            esp_wifi
                            nvs_flash
                            freertos
)
