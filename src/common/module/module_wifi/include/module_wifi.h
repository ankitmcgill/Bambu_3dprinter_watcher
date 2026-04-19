// MODULE_WIFI
// SEPTEMBER 6, 2025

#ifndef _MODULE_WIFI_
#define _MODULE_WIFI_

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include "driver_wifi.h"
#include "util_dataqueue.h"

#define MODULE_WIFI_DATAQUEUE_MAX               (3)
#define MODULE_WIFI_WIFI_CONNECT_TIMEOUT_SEC    (15)
#define MODULE_WIFI_WIFI_CONNECT_RETRY_MAX      (2)
#define MODULE_WIFI_NOTIFICATION_TARGET_MAX     (DRIVER_WIFI_NOTIFICATION_TARGET_MAX)

typedef enum {
    MODULE_WIFI_COMMAND_CONNECT = 0
}module_wifi_command_type_t;

typedef enum{
    MODULE_WIFI_STATE_IDLE = 0,
    MODULE_WIFI_STATE_CHECK_SAVED_CREDENTIALS,
    MODULE_WIFI_STATE_CHECK_DEFAULT_CREDENTIALS,
    MODULE_WIFI_STATE_SCAN,
    MODULE_WIFI_STATE_SCANNING,
    MODULE_WIFI_STATE_SCAN_DONE,
    MODULE_WIFI_STATE_SMARTCONFIG,
    MODULE_WIFI_STATE_SMARTCONFIG_WAITING,
    MODULE_WIFI_STATE_CONNECT,
    MODULE_WIFI_STATE_CONNECTING,
    MODULE_WIFI_STATE_CONNECTED,
    MODULE_WIFI_STATE_GOT_IP,
    MODULE_WIFI_STATE_LOST_IP,
    MODULE_WIFI_STATE_DISCONNECTED
}module_wifi_state_t;

typedef driver_wifi_notification_type_t module_wifi_notification_type_t;

bool MODULE_WIFI_Init(void);

bool MODULE_WIFI_AddCommand(util_dataqueue_item_t* dq_i);
bool MODULE_WIFI_AddNotificationTarget(util_dataqueue_t* dq);

#endif