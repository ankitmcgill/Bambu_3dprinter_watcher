// UTIL DATAQUEUE
// DECEMBER 23, 2025

#include "esp_log.h"

#include "util_dataqueue.h"

// Extern Variables

// Local Variables

// Local Functions

// External Functions
void UTIL_DATAQUEUE_Create(util_dataqueue_t* dq, uint8_t len)
{
    // Create Queue

    dq->handle = xQueueCreate(len, sizeof(util_dataqueue_item_t));
}

bool UTIL_DATAQUEUE_MessageQueue(util_dataqueue_t* dq, util_dataqueue_item_t* i, TickType_t wait)
{
    // Queue Item

    if(uxQueueSpacesAvailable(dq->handle) == 0){
        return false;
    }

    return (xQueueSend(dq->handle, (void*)i, wait) == pdPASS);
}

bool UTIL_DATAQUEUE_MessageCheck(util_dataqueue_t* dq)
{
    // Check For Item

    return (uxQueueMessagesWaiting(dq->handle) == pdPASS);
}

bool UTIL_DATAQUEUE_MessageGet(util_dataqueue_t* dq, util_dataqueue_item_t* i, TickType_t wait)
{
    // Get Item

    if(uxQueueMessagesWaiting(dq->handle) != pdPASS){
        return false;
    }

    return (xQueueReceive(dq->handle, (void*)i, wait) == pdPASS);
}