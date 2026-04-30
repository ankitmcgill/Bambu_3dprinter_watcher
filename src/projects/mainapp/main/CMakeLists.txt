idf_component_register(SRCS "main.c"
                        REQUIRES
                            driver_chipinfo
                            driver_appinfo
                            driver_lcd
                            driver_wifi
                            driver_mqtt
                            driver_nvs
                            module_wifi
                            module_mqtt
                            module_printer
                            util_dataqueue
                            defines
                            project_defines
                            project_ui_export
                        PRIV_REQUIRES
                            spi_flash
                            nvs_flash
                            log
                            freertos
                        INCLUDE_DIRS "")
