// BSP
// SEPTEMBER 30, 2025

#ifndef _BSP_
#define _BSP_

#include "driver/gpio.h"

// BOARD : ESP32-S3-WROOM-1 -No Touch-LCD-4.3 hardware (https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4.3#Introduction)
// LCD DRIVER : ST7262
// SIZE : 4.3 INCH
// RESOLUTION :  800 x 400
// COLOR FORMAT : RGB 565
// ESP32 PSRAM AVAILABLE : 4MB

// LCD PINS
#define BSP_LCD_GPIO_VSYNC      (GPIO_NUM_3)
#define BSP_LCD_GPIO_HSYNC      (GPIO_NUM_46)
#define BSP_LCD_GPIO_DE         (GPIO_NUM_5)
#define BSP_LCD_GPIO_DISP       (-1)
#define BSP_LCD_GPIO_PCLK       (GPIO_NUM_7)
#define BSP_LCD_GPIO_DATA0      (GPIO_NUM_14)   // B3
#define BSP_LCD_GPIO_DATA1      (GPIO_NUM_38)   // B4
#define BSP_LCD_GPIO_DATA2      (GPIO_NUM_18)   // B5
#define BSP_LCD_GPIO_DATA3      (GPIO_NUM_17)   // B6
#define BSP_LCD_GPIO_DATA4      (GPIO_NUM_10)   // B7
#define BSP_LCD_GPIO_DATA5      (GPIO_NUM_39)   // G2
#define BSP_LCD_GPIO_DATA6      (GPIO_NUM_0)    // G3
#define BSP_LCD_GPIO_DATA7      (GPIO_NUM_45)   // G4
#define BSP_LCD_GPIO_DATA8      (GPIO_NUM_48)   // G5
#define BSP_LCD_GPIO_DATA9      (GPIO_NUM_47)   // G6
#define BSP_LCD_GPIO_DATA10     (GPIO_NUM_21)   // G7
#define BSP_LCD_GPIO_DATA11     (GPIO_NUM_1)    // R3
#define BSP_LCD_GPIO_DATA12     (GPIO_NUM_2)    // R4
#define BSP_LCD_GPIO_DATA13     (GPIO_NUM_42)   // R5
#define BSP_LCD_GPIO_DATA14     (GPIO_NUM_41)   // R6
#define BSP_LCD_GPIO_DATA15     (GPIO_NUM_40)   // R7
#define BSP_LCD_GPIO_RST        (-1)

// I2C
#define BSP_I2C_GPIO_SCL        (GPIO_NUM_9)
#define BSP_I2C_GPIO_SDA        (GPIO_NUM_8)

#endif