// MODULE_PRINTER
// APRIL 30, 2026

#ifndef _MODULE_PRINTER_
#define _MODULE_PRINTER_

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include "util_dataqueue.h"

#define MODULE_PRINTER_DATAQUEUE_MAX              (4)
#define MODULE_PRINTER_NOTIFICATION_TARGET_MAX    (2)
#define MODULE_PRINTER_ONLINE_TIMEOUT_SEC         (10)

typedef enum {
    MODULE_PRINTER_STATE_ONLINE = 0,
    MODULE_PRINTER_STATE_OFFLINE,
} module_printer_state_t;

typedef enum {
    MODULE_PRINTER_GCODE_STATUS_IDLE = 0,
    MODULE_PRINTER_GCODE_STATUS_PREPARE,
    MODULE_PRINTER_GCODE_STATUS_RUNNING,
    MODULE_PRINTER_GCODE_STATUS_PAUSE,
    MODULE_PRINTER_GCODE_STATUS_FINISH,
    MODULE_PRINTER_GCODE_STATUS_FAILED
} module_printer_gcode_status_t;

typedef enum {
    MODULE_PRINTER_NOTIFICATION_DATA_CHANGE = 0,
} module_printer_notification_type_t;

typedef struct {
    module_printer_state_t state;
    bool is_dirty_state;
    module_printer_gcode_status_t gcode_status;
    bool is_dirty_gcode_status;
    uint16_t bed_temp;
    bool is_dirty_bed_temp;
    uint16_t bed_temp_target;
    bool is_dirty_bed_temp_target;
    uint16_t nozzle_temp;
    bool is_dirty_nozzle_temp;
    uint16_t nozzle_temp_target;
    bool is_dirty_nozzle_temp_target;
    uint8_t print_progress_percentage;
    bool is_dirty_print_progress_percentage;
    uint16_t current_layer;
    bool is_dirty_current_layer;
    uint16_t total_layer;
    bool is_dirty_total_layer;
    uint32_t time_remaining_s;
    bool is_dirty_time_remaining_s;
} module_printer_parameters_t;

bool MODULE_PRINTER_Init(void);
bool MODULE_PRINTER_Connect(void);

bool MODULE_PRINTER_AddNotificationTarget(util_dataqueue_t* dq);

#endif
