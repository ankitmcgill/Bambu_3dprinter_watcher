// BSP
// SEPTEMBER 30, 2025

#ifndef _BSP_
#define _BSP_

#include "driver/gpio.h"

// LCD DRIVER : ST7789T3
// RESOLUTION : 240 x 320
// COLOR FORMAT : RGB 666 18 BIT
// INTERFACE : SPI

// LCD PINS
#define BSP_LCD_GPIO_RST    (GPIO_NUM_39)
#define BSP_LCD_GPIO_DC     (GPIO_NUM_41)
#define BSP_LCD_GPIO_CS     (GPIO_NUM_42)
#define BSP_LCD_GPIO_SCK    (GPIO_NUM_40)
#define BSP_LCD_GPIO_MOSI   (GPIO_NUM_45)
#define BSP_LCD_GPIO_BL     (GPIO_NUM_5)

#endif