// DRIVER_MQTT
// APRIL 28, 2026

#ifndef _DRIVER_MQTT_
#define _DRIVER_MQTT_

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include "util_dataqueue.h"

#define DRIVER_MQTT_DATAQUEUE_MAX               (4)
#define DRIVER_MQTT_NOTIFICATION_TARGET_MAX     (2)

#define DRIVER_MQTT_USERNAME_LEN_MAX            (32)
#define DRIVER_MQTT_PASSWORD_LEN_MAX            (256)
#define DRIVER_MQTT_URL_LEN_MAX                 (64)
#define DRIVER_MQTT_TOPIC_LEN_MAX               (64)

typedef enum {
    DRIVER_MQTT_COMMAND_CONNECT = 0,
}driver_mqtt_command_type_t;

typedef enum {
    DRIVER_MQTT_NOTIFICATION_CONNECTED = 0,
    DRIVER_MQTT_NOTIFICATION_DISCONNECTED,
    DRIVER_MQTT_NOTIFICATION_GCODE_STATE,
}driver_mqtt_notification_type_t;

typedef enum {
    DRIVER_MQTT_GCODE_STATE_IDLE = 0,
    DRIVER_MQTT_GCODE_STATE_RUNNING,
    DRIVER_MQTT_GCODE_STATE_PAUSE,
    DRIVER_MQTT_GCODE_STATE_FAILED,
    DRIVER_MQTT_GCODE_STATE_FINISH,
    DRIVER_MQTT_GCODE_STATE_UNKNOWN,
}driver_mqtt_gcode_state_t;

bool DRIVER_MQTT_Init(void);

void DRIVER_MQTT_SetCredentials(const char* username, const char* password);
void DRIVER_MQTT_SetBrokerUrl(const char* url);
void DRIVER_MQTT_SetTopic(const char* topic);

bool DRIVER_MQTT_AddCommand(util_dataqueue_item_t* dq_i);
bool DRIVER_MQTT_AddNotificationTarget(util_dataqueue_t* dq);

#endif
