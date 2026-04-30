// MODULE_PRINTER
// APRIL 30, 2026

#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "cJSON.h"

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
static module_printer_parameters_t s_printer_params;

// Local Functions
static void s_notify(module_printer_notification_type_t notification);
static void s_state_set(module_printer_state_t newstate);
static void s_state_mainiter(void);
static void s_task_function(void* pvParameters);
static void s_online_timer_cb(void* arg);
static void s_parse_mqtt_data(const char* json_str);

// External Functions
bool MODULE_PRINTER_Init(void)
{
    // Initialize Module Printer

    s_component_type = COMPONENT_TYPE_TASK;
    s_state = -1;
    s_state_prev = -1;
    s_state_set(MODULE_PRINTER_STATE_OFFLINE);
    memset(&s_printer_params, 0, sizeof(module_printer_parameters_t));

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
    // Send Notification With Current Params Pointer To All Registered Targets

    util_dataqueue_item_t dq_i = {
        .data_type = DATA_TYPE_NOTIFICATION,
        .data      = notification,
    };
    dq_i.data_buff.value.ptr = &s_printer_params;

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
    // Entry actions fire once per state transition

    switch(s_state)
    {
        case MODULE_PRINTER_STATE_ONLINE:
            if(s_state_prev != s_state){
                s_printer_params.state = s_state;
                s_printer_params.is_dirty_state = true;
                s_notify(MODULE_PRINTER_NOTIFICATION_DATA_CHANGE);
                s_state_prev = s_state;
            }
            break;

        case MODULE_PRINTER_STATE_OFFLINE:
            if(s_state_prev != s_state){
                s_printer_params.state = s_state;
                s_printer_params.is_dirty_state = true;
                s_notify(MODULE_PRINTER_NOTIFICATION_DATA_CHANGE);
                s_state_prev = s_state;
            }
            break;

        default:
            break;
    }
}

static void s_parse_mqtt_data(const char* json_str)
{
    // Parse Bambu Lab MQTT JSON And Update s_printer_params

    cJSON* root = cJSON_Parse(json_str);
    if(!root) return;

    cJSON* print = cJSON_GetObjectItem(root, "print");
    if(!print){
        cJSON_Delete(root);
        return;
    }

    bool changed = false;

    cJSON* gcode_state = cJSON_GetObjectItem(print, "gcode_state");
    if(cJSON_IsString(gcode_state)){
        const char* val = gcode_state->valuestring;
        module_printer_gcode_status_t new_status = s_printer_params.gcode_status;
        if     (strcmp(val, "IDLE")    == 0) new_status = MODULE_PRINTER_GCODE_STATUS_IDLE;
        else if(strcmp(val, "PREPARE") == 0) new_status = MODULE_PRINTER_GCODE_STATUS_PREPARE;
        else if(strcmp(val, "RUNNING") == 0) new_status = MODULE_PRINTER_GCODE_STATUS_RUNNING;
        else if(strcmp(val, "PAUSE")   == 0) new_status = MODULE_PRINTER_GCODE_STATUS_PAUSE;
        else if(strcmp(val, "FINISH")  == 0) new_status = MODULE_PRINTER_GCODE_STATUS_FINISH;
        else if(strcmp(val, "FAILED")  == 0) new_status = MODULE_PRINTER_GCODE_STATUS_FAILED;
        if(new_status != s_printer_params.gcode_status){
            s_printer_params.gcode_status = new_status;
            s_printer_params.is_dirty_gcode_status = true;
            changed = true;
        }
    }

    cJSON* nozzle_temper = cJSON_GetObjectItem(print, "nozzle_temper");
    if(cJSON_IsNumber(nozzle_temper)){
        uint16_t val = (uint16_t)roundf((float)nozzle_temper->valuedouble);
        if(val != s_printer_params.nozzle_temp){
            s_printer_params.nozzle_temp = val;
            s_printer_params.is_dirty_nozzle_temp = true;
            changed = true;
        }
    }

    cJSON* nozzle_target_temper = cJSON_GetObjectItem(print, "nozzle_target_temper");
    if(cJSON_IsNumber(nozzle_target_temper)){
        uint16_t val = (uint16_t)roundf((float)nozzle_target_temper->valuedouble);
        if(val != s_printer_params.nozzle_temp_target){
            s_printer_params.nozzle_temp_target = val;
            s_printer_params.is_dirty_nozzle_temp_target = true;
            changed = true;
        }
    }

    cJSON* bed_temper = cJSON_GetObjectItem(print, "bed_temper");
    if(cJSON_IsNumber(bed_temper)){
        uint16_t val = (uint16_t)roundf((float)bed_temper->valuedouble);
        if(val != s_printer_params.bed_temp){
            s_printer_params.bed_temp = val;
            s_printer_params.is_dirty_bed_temp = true;
            changed = true;
        }
    }

    cJSON* bed_target_temper = cJSON_GetObjectItem(print, "bed_target_temper");
    if(cJSON_IsNumber(bed_target_temper)){
        uint16_t val = (uint16_t)roundf((float)bed_target_temper->valuedouble);
        if(val != s_printer_params.bed_temp_target){
            s_printer_params.bed_temp_target = val;
            s_printer_params.is_dirty_bed_temp_target = true;
            changed = true;
        }
    }

    cJSON_Delete(root);

    if(changed){
        s_notify(MODULE_PRINTER_NOTIFICATION_DATA_CHANGE);
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

                        case MODULE_MQTT_NOTIFICATION_DATA_RECEIVED: {
                            char* data = (char*)dq_i.data_buff.value.mqtt_data;
                            // Reset timeout window and go/stay online
                            if(esp_timer_is_active(s_online_timer)){
                                ESP_ERROR_CHECK(esp_timer_stop(s_online_timer));
                            }
                            ESP_ERROR_CHECK(esp_timer_start_once(
                                s_online_timer,
                                (uint64_t)MODULE_PRINTER_ONLINE_TIMEOUT_SEC * 1000000
                            ));
                            s_state_set(MODULE_PRINTER_STATE_ONLINE);
                            if(data){
                                s_parse_mqtt_data(data);
                                free(data);
                            }
                            break;
                        }

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
