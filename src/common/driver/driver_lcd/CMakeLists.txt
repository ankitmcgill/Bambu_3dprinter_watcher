# set(priv_reqs )

# # Include ui In Requires If CONFIG_INCLUDE_UI Is Set (menuconfig -> Project Configurations)
# if(CONFIG_INCLUDE_UI STREQUAL "1")
#     list(APPEND priv_reqs "ui")
#     # set(${priv_reqs} "${${priv_reqs}}" PARENT_SCOPE) 
#     message(WRANING "!!!!!!!!!!!!!!!!!!!!")
# endif()

idf_component_register(SRCS "driver_lcd.c"
                        INCLUDE_DIRS "include"
                        PRIV_REQUIRES esp_lcd esp_timer lvgl__lvgl esp_common defines bsp
                        REQUIRES util_dataqueue
)

# if(CONFIG_INCLUDE_UI)
    idf_component_optional_requires(PRIVATE ui)
# endif()