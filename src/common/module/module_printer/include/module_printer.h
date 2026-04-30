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
    MODULE_PRINTER_NOTIFICATION_ONLINE = 0,
    MODULE_PRINTER_NOTIFICATION_OFFLINE,
} module_printer_notification_type_t;

bool MODULE_PRINTER_Init(void);

bool MODULE_PRINTER_AddNotificationTarget(util_dataqueue_t* dq);

#endif
