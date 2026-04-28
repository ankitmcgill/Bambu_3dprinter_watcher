// DRIVER_LCD
// SEPTEMBER 30, 2025

#ifndef _DRIVER_LCD_
#define _DRIVER_LCD_

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include "util_dataqueue.h"

#define DRIVER_LCD_LVGL_TICK_PERIOD_MS      (2)
#define DRIVER_LCD_LVGL_TASK_PERIOD_MS      (50)

#define DRIVER_LCD_DISPLAY_HRES             (170)
#define DRIVER_LCD_DISPLAY_VRES             (320)

#define DRIVER_LCD_DATAQUEUE_MAX            (4)

#define DRIVER_LCD_SPI_HOST                 SPI2_HOST
#define DRIVER_LCD_SPI_CLK_HZ               (40 * 1000 * 1000)

typedef enum {
    DRIVER_LCD_COMMAND_DEMO = 0,
    DRIVER_LCD_COMMAND_LOAD_UI_SCREEN,
    DRIVER_LCD_COMMAND_REFRESH_UI,
    DRIVER_LCD_COMMAND_SET_SCREEN2_MESSAGE,
}driver_lcd_command_type_t;

typedef enum {
    DRIVER_LCD_SCREEN_STARTUP = 0,
    DRIVER_LCD_SCREEN_HOME
}driver_lcd_screen_type_t;

bool DRIVER_LCD_Init(void);

bool DRIVER_LCD_AddCommand(util_dataqueue_item_t* dq_i);

#endif