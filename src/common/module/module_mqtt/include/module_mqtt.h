// MODULE_MQTT
// APRIL 29, 2026

#ifndef _MODULE_MQTT_
#define _MODULE_MQTT_

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include "driver_mqtt.h"
#include "util_dataqueue.h"

#define MODULE_MQTT_DATAQUEUE_MAX               (4)
#define MODULE_MQTT_NOTIFICATION_TARGET_MAX     (DRIVER_MQTT_NOTIFICATION_TARGET_MAX)
#define MODULE_MQTT_RECONNECT_PERIOD_SEC        (15)

typedef enum {
    MODULE_MQTT_STATE_IDLE = 0,
    MODULE_MQTT_STATE_CONNECTING,
    MODULE_MQTT_STATE_CONNECTED,
    MODULE_MQTT_STATE_DISCONNECTED,
}module_mqtt_state_t;

typedef enum {
    MODULE_MQTT_COMMAND_CONNECT = 0,
    MODULE_MQTT_COMMAND_DISCONNECT,
}module_mqtt_command_type_t;

typedef enum {
    MODULE_MQTT_NOTIFICATION_CONNECTED = 0,
    MODULE_MQTT_NOTIFICATION_DISCONNECTED,
    MODULE_MQTT_NOTIFICATION_DATA_RECEIVED,
}module_mqtt_notification_type_t;

bool MODULE_MQTT_Init(void);

void MODULE_MQTT_SetCredentials(const char* username, const char* password);
void MODULE_MQTT_SetBrokerUrl(const char* url);
void MODULE_MQTT_SetTopic(const char* topic);

bool MODULE_MQTT_AddCommand(util_dataqueue_item_t* dq_i);
bool MODULE_MQTT_AddNotificationTarget(util_dataqueue_t* dq);

#endif
