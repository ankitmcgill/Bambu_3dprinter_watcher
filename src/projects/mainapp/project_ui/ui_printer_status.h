// UI_PRINTER_STATUS
// Printer status screen for 170x320 ST7789 display
// LVGL v9 — requires fonts: montserrat_8, montserrat_10, montserrat_18
//   Enable in menuconfig: Component config → LVGL → Font usage → Montserrat 8 / 10 / 18

#ifndef UI_PRINTER_STATUS_H
#define UI_PRINTER_STATUS_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

typedef struct {
    bool     online;
    int16_t  bed_actual_c;
    int16_t  bed_target_c;
    int16_t  nozzle_actual_c;
    int16_t  nozzle_target_c;
    uint8_t  progress_pct;      // 0-100
    uint16_t layer_current;
    uint16_t layer_total;
    uint8_t  time_h;
    uint8_t  time_m;
    uint8_t  time_s;
} ui_printer_data_t;

// Build the UI on the given parent (pass lv_screen_active() for full screen)
void ui_printer_status_create(lv_obj_t *parent);

// Refresh all displayed values from data
void ui_printer_status_update(const ui_printer_data_t *data);

#endif // UI_PRINTER_STATUS_H
