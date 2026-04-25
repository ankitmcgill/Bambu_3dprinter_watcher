// BSP
// SEPTEMBER 30, 2025

#ifndef _BSP_
#define _BSP_

#include "driver/gpio.h"

// BOARD : ESP32 Display 1.9-inch 170*320 Yellow LCD Board
// https://www.surenoo.com/products/23377371
// https://www.aliexpress.com/item/1005009366691313.html
// LCD DRIVER : ST7789
// SIZE : 1.9 INCH
// RESOLUTION :  170 x 320
// COLOR FORMAT : RGB 565 16 BIT
// INTERFACE : SPI

// LCD PINS
#define BSP_LCD_GPIO_RST    (GPIO_NUM_4)
#define BSP_LCD_GPIO_DC     (GPIO_NUM_2)
#define BSP_LCD_GPIO_CS     (GPIO_NUM_15)
#define BSP_LCD_GPIO_SCK    (GPIO_NUM_14)
#define BSP_LCD_GPIO_MOSI   (GPIO_NUM_13)
#define BSP_LCD_GPIO_BL     (GPIO_NUM_21)

#endif