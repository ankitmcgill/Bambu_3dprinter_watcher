idf_component_register(SRCS "main.c"
                        REQUIRES
                            driver_chipinfo
                            driver_appinfo
                            driver_wifi
                            module_wifi
                            util_dataqueue
                            defines
                            project_defines
                        PRIV_REQUIRES
                            spi_flash
                            nvs_flash
                            log
                            freertos
                        INCLUDE_DIRS "")
