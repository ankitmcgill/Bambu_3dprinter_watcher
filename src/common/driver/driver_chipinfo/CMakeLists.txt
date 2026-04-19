idf_component_register(SRCS "driver_chipinfo.c"
                       INCLUDE_DIRS "include"
                       REQUIRES
                            spi_flash
                       PRIV_REQUIRES
                            esp_hw_support
                            esp_system
                            defines
)
