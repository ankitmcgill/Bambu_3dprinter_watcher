// MODULE_MQTT
// APRIL 29, 2026

#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "module_mqtt.h"
#include "define_common_data_types.h"
#include "define_rtos_tasks.h"

// Extern Variables
TaskHandle_t handle_task_module_mqtt;

// Local Variables
static module_mqtt_state_t s_state;
static module_mqtt_state_t s_state_prev;
static util_dataqueue_t s_dataqueue;
static uint8_t s_notification_targets_count;
static util_dataqueue_t* s_notification_targets[MODULE_MQTT_NOTIFICATION_TARGET_MAX];
static util_dataqueue_item_t s_dq_i;
static rtos_component_type_t s_component_type;
static esp_timer_handle_t s_reconnect_timer_handle;
static esp_timer_handle_t s_printer_timer_handle;

// Local Functions
static bool s_notify(util_dataqueue_item_t* dq_i, TickType_t wait);
static void s_state_set(module_mqtt_state_t newstate);
static void s_state_mainiter(void);
static void s_issue_connect(void);
// Callbacks
static void s_task_function(void *pvParameters);
static void s_timer_cb(void *arg);
static void s_printer_timer_cb(void *arg);

// External Functions
bool MODULE_MQTT_Init(void)
{
    // Initialize Module Mqtt

    s_component_type = COMPONENT_TYPE_TASK;
    s_state = -1;
    s_state_prev = -1;
    s_state_set(MODULE_MQTT_STATE_IDLE);

    // Create Data Queue
    UTIL_DATAQUEUE_Create(&s_dataqueue, MODULE_MQTT_DATAQUEUE_MAX);
    s_notification_targets_count = 0;

    // Create Task
    xTaskCreate(
        s_task_function,
        "t-m-mqtt",
        TASK_STACK_DEPTH_MODULE_MQTT,
        NULL,
        TASK_PRIORITY_MODULE_MQTT,
        &handle_task_module_mqtt
    );

    // Setup Reconnect Timer
    const esp_timer_create_args_t timer_args = {
        .callback = &s_timer_cb,
        .arg = (void*)0,
        .name = "mqtt-reconnect"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_reconnect_timer_handle));

    // Setup Printer Online Timeout Timer
    const esp_timer_create_args_t printer_timer_args = {
        .callback = &s_printer_timer_cb,
        .arg = (void*)0,
        .name = "mqtt-printer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&printer_timer_args, &s_printer_timer_handle));

    // Subscribe To Driver Mqtt Notifications
    DRIVER_MQTT_AddNotificationTarget(&s_dataqueue);

    ESP_LOGI(DEBUG_TAG_MODULE_MQTT, "Type %u. Init", s_component_type);

    return true;
}

void MODULE_MQTT_SetCredentials(const char* username, const char* password)
{
    // Set Mqtt Credentials

    DRIVER_MQTT_SetCredentials(username, password);
}

void MODULE_MQTT_SetBrokerUrl(const char* url)
{
    // Set Mqtt Broker Url

    DRIVER_MQTT_SetBrokerUrl(url);
}

void MODULE_MQTT_SetTopic(const char* topic)
{
    // Set Mqtt Subscribe Topic

    DRIVER_MQTT_SetTopic(topic);
}

bool MODULE_MQTT_AddCommand(util_dataqueue_item_t* dq_i)
{
    // Add Command

    if(!UTIL_DATAQUEUE_MessageQueue(&s_dataqueue, dq_i, 0)){
        ESP_LOGW(DEBUG_TAG_MODULE_MQTT, "Message Queue Failed %s", __FILE__);

        return false;
    }

    return true;
}

bool MODULE_MQTT_AddNotificationTarget(util_dataqueue_t* dq)
{
    // Add Notification Target

    if(s_notification_targets_count >= MODULE_MQTT_NOTIFICATION_TARGET_MAX){
        return false;
    }

    s_notification_targets[s_notification_targets_count] = dq;
    s_notification_targets_count += 1;

    return true;
}

static bool s_notify(util_dataqueue_item_t* dq_i, TickType_t wait)
{
    // Send Notification

    for(uint8_t i = 0; i < s_notification_targets_count; i++){
        if(!UTIL_DATAQUEUE_MessageQueue(s_notification_targets[i], dq_i, wait)){
            ESP_LOGW(DEBUG_TAG_MODULE_MQTT, "Message Queue Failed %s", __FILE__);
        }
    }

    return true;
}

static void s_state_set(module_mqtt_state_t newstate)
{
    // Module Mqtt Set State

    if(s_state == newstate){
        return;
    }

    s_state_prev = s_state;
    s_state = newstate;

    ESP_LOGI(DEBUG_TAG_MODULE_MQTT, "%u -> %u", s_state_prev, s_state);
}

static void s_issue_connect(void)
{
    // Send Connect Command To Driver Mqtt And Transition To Connecting

    util_dataqueue_item_t dq_i = {
        .data_type = DATA_TYPE_COMMAND,
        .data      = DRIVER_MQTT_COMMAND_CONNECT,
    };
    DRIVER_MQTT_AddCommand(&dq_i);
    s_state_set(MODULE_MQTT_STATE_CONNECTING);
}

static void s_state_mainiter(void)
{
    // State Mainiter
    // States are set by driver_mqtt notifications
    // Entry actions (notifications to higher layers) fire once per state transition

    util_dataqueue_item_t out;
    out.data_type = DATA_TYPE_NOTIFICATION;

    switch(s_state)
    {
        case MODULE_MQTT_STATE_IDLE:
            // Do Nothing
            break;

        case MODULE_MQTT_STATE_CONNECTING:
            // Do Nothing - Waiting For DRIVER_MQTT_NOTIFICATION_CONNECTED
            break;

        case MODULE_MQTT_STATE_CONNECTED:
            if(s_state_prev != s_state){
                out.data = MODULE_MQTT_NOTIFICATION_CONNECTED;
                s_notify(&out, 0);

                if(esp_timer_is_active(s_printer_timer_handle)){
                    ESP_ERROR_CHECK(esp_timer_stop(s_printer_timer_handle));
                }
                ESP_ERROR_CHECK(esp_timer_start_once(
                    s_printer_timer_handle,
                    (uint64_t)MODULE_MQTT_PRINTER_ONLINE_TIMEOUT_SEC * 1000000
                ));

                s_state_prev = s_state;
            }
            break;

        case MODULE_MQTT_STATE_DISCONNECTED:
            if(s_state_prev != s_state){
                out.data = MODULE_MQTT_NOTIFICATION_DISCONNECTED;
                s_notify(&out, 0);

                if(esp_timer_is_active(s_printer_timer_handle)){
                    ESP_ERROR_CHECK(esp_timer_stop(s_printer_timer_handle));
                }

                ESP_LOGI(DEBUG_TAG_MODULE_MQTT, "Scheduling reconnect in %us", MODULE_MQTT_RECONNECT_PERIOD_SEC);
                if(esp_timer_is_active(s_reconnect_timer_handle)){
                    ESP_ERROR_CHECK(esp_timer_stop(s_reconnect_timer_handle));
                }
                ESP_ERROR_CHECK(esp_timer_start_once(
                    s_reconnect_timer_handle,
                    (uint64_t)MODULE_MQTT_RECONNECT_PERIOD_SEC * 1000000
                ));

                s_state_prev = s_state;
            }
            break;

        case MODULE_MQTT_STATE_PRINTER_ONLINE:
            if(s_state_prev != s_state){
                out.data = MODULE_MQTT_NOTIFICATION_PRINTER_ONLINE;
                s_notify(&out, 0);
                s_state_prev = s_state;
            }
            break;

        case MODULE_MQTT_STATE_PRINTER_OFFLINE:
            if(s_state_prev != s_state){
                out.data = MODULE_MQTT_NOTIFICATION_PRINTER_OFFLINE;
                s_notify(&out, 0);
                s_state_prev = s_state;
            }
            break;

        default:
            break;
    }
}

static void s_task_function(void *pvParameters)
{
    // Task Function

    ESP_LOGI(DEBUG_TAG_MODULE_MQTT, "Starting task");

    memset(&s_dq_i, 0, sizeof(util_dataqueue_item_t));
    while(true){
        // Check Data Queue
        if(UTIL_DATAQUEUE_MessageCheck(&s_dataqueue))
        {
            if(UTIL_DATAQUEUE_MessageGet(&s_dataqueue, &s_dq_i, 0))
            {
                ESP_LOGI(DEBUG_TAG_MODULE_MQTT, "New In DataQueue. Type %u, Data %u", s_dq_i.data_type, s_dq_i.data);

                if(s_dq_i.data_type == DATA_TYPE_COMMAND)
                {
                    switch(s_dq_i.data)
                    {
                        case MODULE_MQTT_COMMAND_CONNECT:
                            if(esp_timer_is_active(s_reconnect_timer_handle)){
                                ESP_ERROR_CHECK(esp_timer_stop(s_reconnect_timer_handle));
                            }
                            s_issue_connect();
                            break;

                        case MODULE_MQTT_COMMAND_DISCONNECT:
                            if(esp_timer_is_active(s_reconnect_timer_handle)){
                                ESP_ERROR_CHECK(esp_timer_stop(s_reconnect_timer_handle));
                            }
                            if(esp_timer_is_active(s_printer_timer_handle)){
                                ESP_ERROR_CHECK(esp_timer_stop(s_printer_timer_handle));
                            }
                            s_state_set(MODULE_MQTT_STATE_IDLE);
                            break;

                        default:
                            break;
                    }
                }
                else if(s_dq_i.data_type == DATA_TYPE_NOTIFICATION)
                {
                    // States Are Set Based On Notifications Received From Driver Mqtt
                    switch(s_dq_i.data)
                    {
                        case DRIVER_MQTT_NOTIFICATION_CONNECTED:
                            if(esp_timer_is_active(s_reconnect_timer_handle)){
                                ESP_ERROR_CHECK(esp_timer_stop(s_reconnect_timer_handle));
                            }
                            s_state_set(MODULE_MQTT_STATE_CONNECTED);
                            break;

                        case DRIVER_MQTT_NOTIFICATION_DISCONNECTED:
                            s_state_set(MODULE_MQTT_STATE_DISCONNECTED);
                            break;

                        case DRIVER_MQTT_NOTIFICATION_DATA_RECEIVED: {
                            util_dataqueue_item_t out = {
                                .data_type = DATA_TYPE_NOTIFICATION,
                                .data      = MODULE_MQTT_NOTIFICATION_DATA_RECEIVED,
                            };
                            s_notify(&out, 0);

                            if(esp_timer_is_active(s_printer_timer_handle)){
                                ESP_ERROR_CHECK(esp_timer_stop(s_printer_timer_handle));
                            }
                            ESP_ERROR_CHECK(esp_timer_start_once(
                                s_printer_timer_handle,
                                (uint64_t)MODULE_MQTT_PRINTER_ONLINE_TIMEOUT_SEC * 1000000
                            ));
                            s_state_set(MODULE_MQTT_STATE_PRINTER_ONLINE);

                            free(s_dq_i.data_buff.value.mqtt_data);
                            s_dq_i.data_buff.value.mqtt_data = NULL;
                            break;
                        }

                        default:
                            break;
                    }
                }
            }
        }

        // Run State Mainiter
        s_state_mainiter();

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    vTaskDelete(NULL);
}

static void s_timer_cb(void *arg)
{
    // Reconnect Timer Callback
    // Triggered After Reconnect Delay; Re-issues Connect To Driver

    ESP_LOGI(DEBUG_TAG_MODULE_MQTT, "Reconnect timer fired");
    s_issue_connect();
}

static void s_printer_timer_cb(void *arg)
{
    // Printer Online Timeout Callback
    // Triggered When No Data Received Within MODULE_MQTT_PRINTER_ONLINE_TIMEOUT_SEC

    ESP_LOGI(DEBUG_TAG_MODULE_MQTT, "Printer online timer expired");
    s_state_set(MODULE_MQTT_STATE_PRINTER_OFFLINE);
}
