idf_component_register(SRCS "driver_appinfo.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES
                            esp_common 
                            defines
                       REQUIRES
                            esp_app_format
)
