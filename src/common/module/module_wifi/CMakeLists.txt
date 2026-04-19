idf_component_register(SRCS "module_wifi.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES
                            driver_wifi
                            util_dataqueue
                            defines
                            project_defines
                       REQUIRES
                            esp_wifi
                            esp_timer
                            nvs_flash
                            freertos
)
