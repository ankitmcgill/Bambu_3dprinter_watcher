// UTIL DATAQUEUE
// DECEMBER 23, 2025

#ifndef _UTIL_DATAQUEUE_
#define _UTIL_DATAQUEUE_

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum{
    DATA_TYPE_COMMAND = 0,
    DATA_TYPE_NOTIFICATION
}util_dataqueue_data_type_t;

typedef struct{
    union{
        char ip[16];
        struct{
            uint32_t timestamp;
            char time_string[16];
            char am_pm_string[3];
            char date_string[48];
        }timedata;
        struct{
            char humidity[8];
            char temp[8];
        }weatherdata;
        char location[16];
    }value;
}util_dataqueue_data_buffer_type_t;

typedef struct
{
    uint8_t data;
    util_dataqueue_data_type_t data_type;
    util_dataqueue_data_buffer_type_t data_buff;
}util_dataqueue_item_t;

typedef struct
{
    QueueHandle_t handle;
}util_dataqueue_t;

void UTIL_DATAQUEUE_Create(util_dataqueue_t* dq, uint8_t len);
bool UTIL_DATAQUEUE_MessageQueue(util_dataqueue_t* dq, util_dataqueue_item_t* i, TickType_t wait);
bool UTIL_DATAQUEUE_MessageCheck(util_dataqueue_t* dq);
bool UTIL_DATAQUEUE_MessageGet(util_dataqueue_t* dq, util_dataqueue_item_t* i, TickType_t wait);

#endif