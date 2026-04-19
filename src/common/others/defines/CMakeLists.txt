idf_component_register(SRCS
                        PRIV_REQUIRES log
                        REQUIRES
                            freertos
                            driver_wifi
                            module_wifi
                        INCLUDE_DIRS "include")
