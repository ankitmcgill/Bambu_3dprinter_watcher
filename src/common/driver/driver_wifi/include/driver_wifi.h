// DRIVER_WIFI
// SEPTEMBER 6, 2025

#ifndef _DRIVER_WIFI_
#define _DRIVER_WIFI_

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include "esp_wifi.h"
#include "util_dataqueue.h"

#define DRIVER_WIFI_LEN_SSID_MAX                (32)
#define DRIVER_WIFI_LEN_PWD_MAX                 (64)

#define DRIVER_WIFI_SCAN_RESULTS_COUNT_MAX      (10)

#define DRIVER_WIFI_DATAQUEUE_MAX               (3)
#define DRIVER_WIFI_NOTIFICATION_TARGET_MAX     (2)

#define DRIVER_WIFI_HOSTNAME_PREFIX             "ESP32-LVGL"

typedef enum {
    DRIVER_WIFI_COMMAND_SCAN = 0,
    DRIVER_WIFI_COMMAND_SMARTCONFIG,
    DRIVER_WIFI_COMMAND_CONNECT,
    DRIVER_WIFI_COMMAND_DISCONNECT
}driver_wifi_command_type_t;

typedef enum{
    DRIVER_WIFI_NOTIFICATION_SCAN_DONE = 0,
    DRIVER_WIFI_NOTIFICATION_SMARTCONFIG_GOT_CREDENTIALS,
    DRIVER_WIFI_NOTIFICATION_CONNECTED,
    DRIVER_WIFI_NOTIFICATION_GOT_IP,
    DRIVER_WIFI_NOTIFICATION_LOST_IP,
    DRIVER_WIFI_NOTIFICATION_DISCONNECTED
}driver_wifi_notification_type_t;

bool DRIVER_WIFI_Init(void);

bool DRIVER_WIFI_CheckSavedWifiCredentials(void);
void DRIVER_WIFI_SetWifiCredentials(uint8_t* ssid, uint8_t* pwd);

bool DRIVER_WIFI_AddCommand(util_dataqueue_item_t* dq_i);
bool DRIVER_WIFI_AddNotificationTarget(util_dataqueue_t* dq);

#endif