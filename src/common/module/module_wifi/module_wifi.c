// MODULE_WIFI
// SEPTEMBER 6, 2025

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#include "module_wifi.h"
#include "define_common_data_types.h"
#include "define_rtos_tasks.h"
#include "project_defines.h"

// Extern Variables
TaskHandle_t handle_task_module_wifi;

// Local Variables
static module_wifi_state_t s_state;
static module_wifi_state_t s_state_prev;
static util_dataqueue_t s_dataqueue;
static uint8_t s_notification_targets_count;
static util_dataqueue_t* s_notification_targets[MODULE_WIFI_NOTIFICATION_TARGET_MAX];
static module_wifi_state_t s_wifi_credential_source;
static rtos_component_type_t s_component_type;
static esp_timer_handle_t s_wifi_timer_handle;
static uint8_t s_wifi_retry_count;

// Local Functions
static bool s_notify(util_dataqueue_item_t* dq_i, TickType_t wait);
static void s_state_set(module_wifi_state_t newstate);
static void s_state_mainiter(void);
static void s_task_function(void *pvParameters);
static void s_timer_cb(void *arg);

// External Functions
bool MODULE_WIFI_Init(void)
{
    // Initialize Module Wifi

    s_component_type = COMPONENT_TYPE_TASK;
    s_state = -1;
    s_state_prev = -1;
    s_state_set(MODULE_WIFI_STATE_IDLE);

    // Create Data Queue
    UTIL_DATAQUEUE_Create(&s_dataqueue, MODULE_WIFI_DATAQUEUE_MAX);
    s_notification_targets_count = 0;

    // Create Task
    xTaskCreate(
        s_task_function,
        "t-m-wifi",
        TASK_STACK_DEPTH_MODULE_WIFI,
        NULL,
        TASK_PRIORITY_MODULE_WIFI,
        &handle_task_module_wifi
    );

    // Setup Wifi Connect Timer
    const esp_timer_create_args_t timer_args = {
        .callback = &s_timer_cb,
        .arg = (void*)0,     // optional user data
        .name = "one-shot"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_wifi_timer_handle));

    // Add Notification Targets
    DRIVER_WIFI_AddNotificationTarget(&s_dataqueue);

    ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "Type %u. Init", s_component_type);

    return true;
}

bool MODULE_WIFI_AddCommand(util_dataqueue_item_t* dq_i)
{
    // Add Command

    if(!UTIL_DATAQUEUE_MessageQueue(&s_dataqueue, dq_i, 0)){
        ESP_LOGW(DEBUG_TAG_MODULE_WIFI, "Message Queue Failed %s", __FILE__);

        return false;
    }
    
    return true;
}

bool MODULE_WIFI_AddNotificationTarget(util_dataqueue_t* dq)
{
    // Add Notification Target

    if(s_notification_targets_count >= MODULE_WIFI_NOTIFICATION_TARGET_MAX){
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
            ESP_LOGW(DEBUG_TAG_MODULE_WIFI, "Message Queue Failed %s", __FILE__);
        }
    }

    return true;
}

static void s_state_set(module_wifi_state_t newstate)
{
    // Module Wifi Set State
    
    if(s_state == newstate){
        return;
    }

    s_state_prev = s_state;
    s_state = newstate;

    ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "%u -> %u", s_state_prev, s_state);
}

static void s_state_mainiter(void)
{
    // State Mainiter
    
    util_dataqueue_item_t dq_i;
    
    switch(s_state)
    {
        case MODULE_WIFI_STATE_IDLE:
            // Do Nothing
            break;

        case MODULE_WIFI_STATE_CHECK_SAVED_CREDENTIALS:
            s_wifi_retry_count = 0;
            if(DRIVER_WIFI_CheckSavedWifiCredentials())
            {
                // WiFi Connect
                s_wifi_credential_source = MODULE_WIFI_STATE_CHECK_SAVED_CREDENTIALS;
                s_wifi_retry_count = MODULE_WIFI_WIFI_CONNECT_RETRY_MAX;
                s_state_set(MODULE_WIFI_STATE_CONNECT);
                break;
            }

            // No Saved Credentials
            ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "No Saved Wi-Fi Credential Found");
            s_state_set(MODULE_WIFI_STATE_CHECK_DEFAULT_CREDENTIALS);
            break;

        case MODULE_WIFI_STATE_CHECK_DEFAULT_CREDENTIALS:
            s_wifi_retry_count = 0;
            #if defined(DEFAULT_WIFI_SSID) && defined(DEFAULT_WIFI_PASSWORD)
                    ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "Default Wi-Fi Credential Found");
                    ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "   SSID: %s", DEFAULT_WIFI_SSID);
                    ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "   Password: %s", DEFAULT_WIFI_PASSWORD);

                    // Set Wifi Credentials
                    DRIVER_WIFI_SetWifiCredentials((uint8_t*)DEFAULT_WIFI_SSID, (uint8_t*)DEFAULT_WIFI_PASSWORD);

                    // WiFi Connect
                    s_wifi_credential_source = MODULE_WIFI_STATE_CHECK_DEFAULT_CREDENTIALS;
                    s_wifi_retry_count = MODULE_WIFI_WIFI_CONNECT_RETRY_MAX;
                    s_state_set(MODULE_WIFI_STATE_CONNECT);
                    break;
            #endif

            // No Default Credentials
            ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "No Default Wi-Fi Credential Found");
            s_state_set(MODULE_WIFI_STATE_SCAN);
            break;

        case MODULE_WIFI_STATE_SCAN:
            s_wifi_retry_count = 0;
            dq_i.data_type = DATA_TYPE_COMMAND;
            dq_i.data = DRIVER_WIFI_COMMAND_SCAN;
            DRIVER_WIFI_AddCommand(&dq_i);
            s_state_set(MODULE_WIFI_STATE_SCANNING);
            break;
        
        case MODULE_WIFI_STATE_SCANNING:
            // Do Nothing
            break;
        
        case MODULE_WIFI_STATE_SCAN_DONE:
            s_state_set(MODULE_WIFI_STATE_SMARTCONFIG);
            break;

        case MODULE_WIFI_STATE_SMARTCONFIG:
            dq_i.data_type = DATA_TYPE_COMMAND;
            dq_i.data = DRIVER_WIFI_COMMAND_SMARTCONFIG;
            DRIVER_WIFI_AddCommand(&dq_i);
            s_state_set(MODULE_WIFI_STATE_SMARTCONFIG_WAITING);
            break;
        
            case MODULE_WIFI_STATE_SMARTCONFIG_WAITING:
                // Do Nothing
                break;
        
        case MODULE_WIFI_STATE_CONNECT:
            dq_i.data_type = DATA_TYPE_COMMAND;
            dq_i.data = DRIVER_WIFI_COMMAND_CONNECT;
            DRIVER_WIFI_AddCommand(&dq_i);

            // Start Timer
            ESP_ERROR_CHECK(esp_timer_start_once(s_wifi_timer_handle, MODULE_WIFI_WIFI_CONNECT_TIMEOUT_SEC * 1000000));
            s_state_set(MODULE_WIFI_STATE_CONNECTING);
            break;

        case MODULE_WIFI_STATE_CONNECTING:
            // Do Nothing
            break;
        
        case MODULE_WIFI_STATE_CONNECTED:
            // Do Nothing
            break;

        case MODULE_WIFI_STATE_GOT_IP:
            // Stop Timer
            if(esp_timer_is_active(s_wifi_timer_handle)){
                ESP_ERROR_CHECK(esp_timer_stop(s_wifi_timer_handle));
            }
            s_state_set(MODULE_WIFI_STATE_IDLE);
            break;
        
        case MODULE_WIFI_STATE_LOST_IP:
        case MODULE_WIFI_STATE_DISCONNECTED:
            // Lost IP
            // Restart Connection Logic
            // Start New Connection Attempt
            if(esp_timer_is_active(s_wifi_timer_handle)){
                ESP_ERROR_CHECK(esp_timer_stop(s_wifi_timer_handle));
            }
            s_state_set(MODULE_WIFI_STATE_CHECK_SAVED_CREDENTIALS);
            break;
        
            default:
                break;
    }
}

static void s_task_function(void *pvParameters)
{
    // Task Function

    util_dataqueue_item_t dq_i;

    ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "Starting task");

    while(true){
        // Check Data Queue
        if(UTIL_DATAQUEUE_MessageCheck(&s_dataqueue))
        {
            if(UTIL_DATAQUEUE_MessageGet(&s_dataqueue, &dq_i, 0))
            {
                ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "New In DataQueue. Type %u, Data %u", dq_i.data_type, dq_i.data);
                
                if(dq_i.data_type == DATA_TYPE_COMMAND)
                {
                    switch(dq_i.data)
                    {
                        case MODULE_WIFI_COMMAND_CONNECT:
                            s_state_set(MODULE_WIFI_STATE_CHECK_SAVED_CREDENTIALS);
                            break;

                        default:
                            break;
                    }
                }
                else if(dq_i.data_type == DATA_TYPE_NOTIFICATION)
                {
                    // Pass Notification To Module Wifi Notification Targets
                    s_notify(&dq_i, 0);

                    // Take Action On Notification
                    switch(dq_i.data)
                    {
                        case DRIVER_WIFI_NOTIFICATION_SCAN_DONE:
                            s_state_set(MODULE_WIFI_STATE_SCAN_DONE);
                            break;

                        case DRIVER_WIFI_NOTIFICATION_SMARTCONFIG_GOT_CREDENTIALS:
                            s_state_set(MODULE_WIFI_STATE_CONNECT);
                            break;
                        
                        case DRIVER_WIFI_NOTIFICATION_CONNECTED:
                            s_state_set(MODULE_WIFI_STATE_CONNECTED);
                            break;
                        
                        case DRIVER_WIFI_NOTIFICATION_GOT_IP:
                            s_state_set(MODULE_WIFI_STATE_GOT_IP);
                            break;
                        
                        case DRIVER_WIFI_NOTIFICATION_LOST_IP:
                            // Ignore If Not In Idle State
                            if(s_state == MODULE_WIFI_STATE_IDLE){
                                s_state_set(MODULE_WIFI_STATE_LOST_IP);
                            }
                            break;

                        case DRIVER_WIFI_NOTIFICATION_DISCONNECTED:
                            // Ignore If Not In Idle State
                            if(s_state == MODULE_WIFI_STATE_IDLE){
                                s_state_set(MODULE_WIFI_STATE_DISCONNECTED);
                            }
                            break;
                        
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
    // Timer Callback

    if(s_wifi_retry_count <= MODULE_WIFI_WIFI_CONNECT_RETRY_MAX){
        s_wifi_retry_count += 1;
        s_state_set(MODULE_WIFI_STATE_CONNECT);
        return;
    }

    // Retries Exhausted
    // Resume Wifi Credentials State Machine
    ESP_LOGI(DEBUG_TAG_MODULE_WIFI, "%u Connect Retries Exhausted", MODULE_WIFI_WIFI_CONNECT_RETRY_MAX);
    switch(s_wifi_credential_source)
    {
        case MODULE_WIFI_STATE_CHECK_SAVED_CREDENTIALS:
            s_state_set(MODULE_WIFI_STATE_CHECK_DEFAULT_CREDENTIALS);
            break;
        
        case MODULE_WIFI_STATE_CHECK_DEFAULT_CREDENTIALS:
            s_state_set(MODULE_WIFI_STATE_SCAN);
            break;
        
        case MODULE_WIFI_STATE_SMARTCONFIG:
            break;
        
        default:
            s_state_set(MODULE_WIFI_STATE_CHECK_SAVED_CREDENTIALS);
            break;
    }
}