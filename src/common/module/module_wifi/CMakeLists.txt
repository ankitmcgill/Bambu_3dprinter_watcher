idf_component_register(SRCS "module_wifi.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES
                            driver_wifi
                            driver_lcd
                            util_dataqueue
                            defines
                            project_defines
                            esp_http_server
                            lwip
                       REQUIRES
                            esp_wifi
                            esp_timer
                            nvs_flash
                            freertos
)
