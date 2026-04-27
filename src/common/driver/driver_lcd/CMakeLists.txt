idf_component_register(SRCS "driver_lcd.c"
                        INCLUDE_DIRS "include"
                        PRIV_REQUIRES  esp_lcd esp_timer lvgl__lvgl esp_common defines bsp driver
                        REQUIRES util_dataqueue
)

idf_component_optional_requires(PRIVATE project_ui_export)

idf_build_get_property(build_components BUILD_COMPONENTS)
if("project_ui_export" IN_LIST build_components)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE DRIVER_LCD_HAS_PROJECT_UI=1)
endif()