// MODULE_PRINTER
// APRIL 30, 2026

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "module_printer.h"
#include "module_mqtt.h"
#include "define_common_data_types.h"
#include "define_rtos_tasks.h"

// Extern Variables
TaskHandle_t handle_task_module_printer;

// Local Variables
static module_printer_state_t s_state;
static module_printer_state_t s_state_prev;
static util_dataqueue_t s_dataqueue;
static uint8_t s_notification_targets_count;
static util_dataqueue_t* s_notification_targets[MODULE_PRINTER_NOTIFICATION_TARGET_MAX];
static rtos_component_type_t s_component_type;
static esp_timer_handle_t s_online_timer;

// Local Functions
static void s_notify(module_printer_notification_type_t notification);
static void s_state_set(module_printer_state_t newstate);
static void s_state_mainiter(void);
static void s_task_function(void* pvParameters);
static void s_online_timer_cb(void* arg);

// External Functions
bool MODULE_PRINTER_Init(void)
{
    // Initialize Module Printer

    s_component_type = COMPONENT_TYPE_TASK;
    s_state = -1;
    s_state_prev = -1;
    s_state_set(MODULE_PRINTER_STATE_OFFLINE);

    UTIL_DATAQUEUE_Create(&s_dataqueue, MODULE_PRINTER_DATAQUEUE_MAX);
    s_notification_targets_count = 0;

    xTaskCreate(
        s_task_function,
        "t-m-printer",
        TASK_STACK_DEPTH_MODULE_PRINTER,
        NULL,
        TASK_PRIORITY_MODULE_PRINTER,
        &handle_task_module_printer
    );

    const esp_timer_create_args_t timer_args = {
        .callback = s_online_timer_cb,
        .name     = "printer-online"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_online_timer));

    // Subscribe to module_mqtt notifications
    MODULE_MQTT_AddNotificationTarget(&s_dataqueue);

    ESP_LOGI(DEBUG_TAG_MODULE_PRINTER, "Type %u. Init", s_component_type);

    return true;
}

bool MODULE_PRINTER_AddNotificationTarget(util_dataqueue_t* dq)
{
    // Add Notification Target

    if(s_notification_targets_count >= MODULE_PRINTER_NOTIFICATION_TARGET_MAX){
        return false;
    }

    s_notification_targets[s_notification_targets_count] = dq;
    s_notification_targets_count += 1;

    return true;
}

static void s_notify(module_printer_notification_type_t notification)
{
    // Send Notification To All Registered Targets

    util_dataqueue_item_t dq_i = {
        .data_type = DATA_TYPE_NOTIFICATION,
        .data      = notification,
    };

    for(uint8_t i = 0; i < s_notification_targets_count; i++){
        if(!UTIL_DATAQUEUE_MessageQueue(s_notification_targets[i], &dq_i, 0)){
            ESP_LOGW(DEBUG_TAG_MODULE_PRINTER, "Notify queue full");
        }
    }
}

static void s_state_set(module_printer_state_t newstate)
{
    // Set State

    if(s_state == newstate){
        return;
    }

    s_state_prev = s_state;
    s_state = newstate;

    ESP_LOGI(DEBUG_TAG_MODULE_PRINTER, "%u -> %u", s_state_prev, s_state);
}

static void s_state_mainiter(void)
{
    // State Mainiter
    // Entry actions fire once per state transition

    switch(s_state)
    {
        case MODULE_PRINTER_STATE_ONLINE:
            if(s_state_prev != s_state){
                s_notify(MODULE_PRINTER_NOTIFICATION_ONLINE);
                s_state_prev = s_state;
            }
            break;

        case MODULE_PRINTER_STATE_OFFLINE:
            if(s_state_prev != s_state){
                s_notify(MODULE_PRINTER_NOTIFICATION_OFFLINE);
                s_state_prev = s_state;
            }
            break;

        default:
            break;
    }
}

static void s_task_function(void* pvParameters)
{
    // Task Function

    util_dataqueue_item_t dq_i;

    ESP_LOGI(DEBUG_TAG_MODULE_PRINTER, "Starting task");

    while(true)
    {
        if(UTIL_DATAQUEUE_MessageCheck(&s_dataqueue))
        {
            if(UTIL_DATAQUEUE_MessageGet(&s_dataqueue, &dq_i, 0))
            {
                ESP_LOGI(DEBUG_TAG_MODULE_PRINTER, "New In DataQueue. Type %u, Data %u", dq_i.data_type, dq_i.data);

                if(dq_i.data_type == DATA_TYPE_NOTIFICATION)
                {
                    switch(dq_i.data)
                    {
                        case MODULE_MQTT_NOTIFICATION_CONNECTED:
                            // MQTT connected — start timeout; printer must respond within window
                            if(esp_timer_is_active(s_online_timer)){
                                ESP_ERROR_CHECK(esp_timer_stop(s_online_timer));
                            }
                            ESP_ERROR_CHECK(esp_timer_start_once(
                                s_online_timer,
                                (uint64_t)MODULE_PRINTER_ONLINE_TIMEOUT_SEC * 1000000
                            ));
                            break;

                        case MODULE_MQTT_NOTIFICATION_DISCONNECTED:
                            // MQTT lost — stop timer and go offline immediately
                            if(esp_timer_is_active(s_online_timer)){
                                ESP_ERROR_CHECK(esp_timer_stop(s_online_timer));
                            }
                            s_state_set(MODULE_PRINTER_STATE_OFFLINE);
                            break;

                        case MODULE_MQTT_NOTIFICATION_DATA_RECEIVED:
                            // Data from printer — reset timeout window and go/stay online
                            if(esp_timer_is_active(s_online_timer)){
                                ESP_ERROR_CHECK(esp_timer_stop(s_online_timer));
                            }
                            ESP_ERROR_CHECK(esp_timer_start_once(
                                s_online_timer,
                                (uint64_t)MODULE_PRINTER_ONLINE_TIMEOUT_SEC * 1000000
                            ));
                            s_state_set(MODULE_PRINTER_STATE_ONLINE);
                            break;

                        default:
                            break;
                    }
                }
            }
        }

        s_state_mainiter();

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    vTaskDelete(NULL);
}

static void s_online_timer_cb(void* arg)
{
    // Online Timeout Callback
    // Fired when no data received within MODULE_PRINTER_ONLINE_TIMEOUT_SEC

    ESP_LOGI(DEBUG_TAG_MODULE_PRINTER, "Online timer expired — printer offline");
    s_state_set(MODULE_PRINTER_STATE_OFFLINE);
}
