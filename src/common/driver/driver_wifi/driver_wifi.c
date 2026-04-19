// DRIVER_WIFI
// SEPTEMBER 6, 2025

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_smartconfig.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "driver_wifi.h"
#include "driver_chipinfo.h"
#include "util_dataqueue.h"
#include "define_common_data_types.h"
#include "define_rtos_tasks.h"

// Extern Variables
TaskHandle_t handle_task_driver_wifi;

// Local Variables
static rtos_component_type_t s_component_type;
static util_dataqueue_t s_dataqueue;
static uint8_t s_notification_targets_count;
static util_dataqueue_t* s_notification_targets[DRIVER_WIFI_NOTIFICATION_TARGET_MAX];
static char s_ssid[DRIVER_WIFI_LEN_SSID_MAX];
static char s_password[DRIVER_WIFI_LEN_PWD_MAX];
static uint16_t s_scan_ap_count;
static wifi_ap_record_t* s_scan_ap_records;

// Local Functions
static void s_wifi_connect(void);
static void s_wifi_disconnect(void);
static bool s_notify(util_dataqueue_item_t* dq_i, TickType_t wait);
static void s_task_function(void *pvParameters);
static void s_event_handler_wifi(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void s_event_handler_smartconfig(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

// External Functions
bool DRIVER_WIFI_Init(void)
{
    // Initialize Driver Wifi

    esp_netif_t* sta_netif;
    uint8_t wifi_mac[6];
    char hostname[24];

    // Create Data Queue
    UTIL_DATAQUEUE_Create(&s_dataqueue, DRIVER_WIFI_DATAQUEUE_MAX);
    s_notification_targets_count = 0;

    s_component_type = COMPONENT_TYPE_TASK;

    // Allocate Space For Scan AP Records
    s_scan_ap_records = (wifi_ap_record_t*)malloc(DRIVER_WIFI_SCAN_RESULTS_COUNT_MAX * sizeof(wifi_ap_record_t));
    assert(s_scan_ap_records);

    // Initialize NVS
    ESP_ERROR_CHECK(nvs_flash_init());
    esp_wifi_set_storage(WIFI_STORAGE_FLASH);

    // Initialize WiFi And TcpIP
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();

    // Set Hostname
    DRIVER_CHIPINFO_GetChipID((uint8_t*)&wifi_mac);
    sprintf(hostname, 
        "%s-%02X%02X%02X", 
        DRIVER_WIFI_HOSTNAME_PREFIX, wifi_mac[3], wifi_mac[4], wifi_mac[5]);
    ESP_ERROR_CHECK(esp_netif_set_hostname(sta_netif, hostname));

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

    // Start In Station Mode
    esp_wifi_start();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Register Event Handlers
    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &s_event_handler_wifi,
        NULL
    ));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT,
        ESP_EVENT_ANY_ID,
        &s_event_handler_wifi,
        NULL
    ));
    ESP_ERROR_CHECK(esp_event_handler_register(
        SC_EVENT, 
        ESP_EVENT_ANY_ID, 
        &s_event_handler_smartconfig, 
        NULL
    ));

    // Create Driver Wifi Task
    xTaskCreate(
        s_task_function,
        "t-d-wifi",
        TASK_STACK_DEPTH_DRIVER_WIFI,
        NULL,
        TASK_PRIORITY_DRIVER_WIFI,
        &handle_task_driver_wifi
    );

    ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "Type %u. Init. Hostname %s", s_component_type, hostname);

    return true;
}

bool DRIVER_WIFI_CheckSavedWifiCredentials(void)
{
    // Check If Saved Wifi Credentials Are Present

    wifi_config_t wifi_cfg;
    esp_err_t err;

    memset(&wifi_cfg, 0, sizeof(wifi_cfg));
    err = esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg);
    if (err == ESP_OK && strlen((char *)wifi_cfg.sta.ssid) > 0) {
        ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "Saved Wi-Fi Credential Found");
        ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "   SSID: %s", wifi_cfg.sta.ssid);
        ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "   Password: %s", wifi_cfg.sta.password);

        // Set Credentials
        DRIVER_WIFI_SetWifiCredentials(wifi_cfg.sta.ssid, wifi_cfg.sta.password);

        return true;
    }

    return false;
}

void DRIVER_WIFI_SetWifiCredentials(uint8_t* ssid, uint8_t* pwd)
{
    // Save Wifi Credentials

    memcpy(s_ssid, ssid, DRIVER_WIFI_LEN_SSID_MAX);
    memcpy(s_password, pwd, DRIVER_WIFI_LEN_PWD_MAX);
}

bool DRIVER_WIFI_AddCommand(util_dataqueue_item_t* dq_i)
{
    // Add Command

    if(!UTIL_DATAQUEUE_MessageQueue(&s_dataqueue, dq_i, 0)){
        ESP_LOGW(DEBUG_TAG_DRIVER_WIFI, "Message Queue Failed %s", __FILE__);

        return false;
    }

    return true;
}

bool DRIVER_WIFI_AddNotificationTarget(util_dataqueue_t* dq)
{
    // Add Notification Target

    if(s_notification_targets_count >= DRIVER_WIFI_NOTIFICATION_TARGET_MAX){
        return false;
    }

    s_notification_targets[s_notification_targets_count] = dq;
    s_notification_targets_count += 1;

    return true;
}

static void s_wifi_connect(void)
{
    // Connect Wifi
    // Assumes Target Wifi Credentials Are Present In s_ssid & s_password

    wifi_config_t w_config = {
        .sta = {
            .ssid = "",
            .password = ""
        }
    };
    strcpy((char*)w_config.sta.ssid, s_ssid);
    strcpy((char*)w_config.sta.password, s_password);

    ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "Connecting...");

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &w_config));
    esp_wifi_disconnect();
    esp_wifi_connect();
}

static void s_wifi_disconnect(void)
{
    // Disconnect Wifi

    ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "Disconnecting...");

    esp_wifi_disconnect();
}

static bool s_notify(util_dataqueue_item_t* dq_i, TickType_t wait)
{
    // Send Notification

    for(uint8_t i = 0; i < s_notification_targets_count; i++){
        if(!UTIL_DATAQUEUE_MessageQueue(s_notification_targets[i], dq_i, wait)){
            ESP_LOGW(DEBUG_TAG_DRIVER_WIFI, "Message Queue Failed %s", __FILE__);
        }
    }

    return true;
}

static void s_task_function(void *pvParameters)
{
    // Task Function

    util_dataqueue_item_t dq_i;
    wifi_scan_config_t wifi_scan_config;

    ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "Starting task");

    while(true){
        // Check Data Queue
        if(UTIL_DATAQUEUE_MessageCheck(&s_dataqueue))
        {
            if(UTIL_DATAQUEUE_MessageGet(&s_dataqueue, &dq_i, 0))
            {
                ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "New In DataQueue. Type %u, Data %u", dq_i.data_type, dq_i.data);
                
                if(dq_i.data_type == DATA_TYPE_COMMAND)
                {
                    switch(dq_i.data)
                    {
                        case DRIVER_WIFI_COMMAND_SCAN:
                            wifi_scan_config.ssid = NULL;
                            wifi_scan_config.bssid = NULL;
                            wifi_scan_config.channel = 0;
                            wifi_scan_config.show_hidden = false;
                            wifi_scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE,
                            wifi_scan_config.scan_time.active.min = 100;
                            wifi_scan_config.scan_time.active.max = 300;

                            ESP_ERROR_CHECK(esp_wifi_scan_start(&wifi_scan_config, false));
                            break;

                        case DRIVER_WIFI_COMMAND_SMARTCONFIG:
                            smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
                            ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
                            break;

                        case DRIVER_WIFI_COMMAND_CONNECT:
                            s_wifi_connect();
                            break;
                        
                        case DRIVER_WIFI_COMMAND_DISCONNECT:
                            s_wifi_disconnect();
                            break;
                        
                        default:
                            break;
                    }
                }
                else if(dq_i.data_type == DATA_TYPE_NOTIFICATION)
                {
                    // Do Nothing
                    // No Notification Expected For This Module
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    };

    vTaskDelete(NULL);
}

static void s_event_handler_wifi(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    // Wifi Event Handler

    util_dataqueue_item_t dq_i;
    dq_i.data_type = DATA_TYPE_NOTIFICATION;

    if(event_base == WIFI_EVENT)
    {
        switch(event_id)
        {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "WIFI_EVENT_STA_START");
                break;
            
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "WIFI_EVENT_STA_CONNECTED");

                dq_i.data = DRIVER_WIFI_NOTIFICATION_CONNECTED;
                s_notify(&dq_i, 0);
                break;
            
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "WIFI_EVENT_STA_DISCONNECTED");

                dq_i.data = DRIVER_WIFI_NOTIFICATION_DISCONNECTED;
                s_notify(&dq_i, 0);
                break;
            
            case WIFI_EVENT_SCAN_DONE:
                ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "WIFI_EVENT_SCAN_DONE");

                s_scan_ap_count = DRIVER_WIFI_SCAN_RESULTS_COUNT_MAX;
                ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&s_scan_ap_count, s_scan_ap_records));
                
                ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "%u AP Found", s_scan_ap_count);

                for (int i = 0; i < s_scan_ap_count; i++) {
                    ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "[%02d] SSID: %-32s | RSSI: %d | Channel: %d | Auth: %d",
                        i + 1,
                        s_scan_ap_records[i].ssid,
                        s_scan_ap_records[i].rssi,
                        s_scan_ap_records[i].primary,
                        s_scan_ap_records[i].authmode);
                };

                dq_i.data = DRIVER_WIFI_NOTIFICATION_SCAN_DONE;
                s_notify(&dq_i, 0);
                break;
            
            default:
                break;
        }

        return;
    }

    if(event_base == IP_EVENT)
    {
        switch(event_id)
        {
            case IP_EVENT_STA_GOT_IP:
                ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "IP_EVENT_STA_GOT_IP");
                
                ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
                ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "IP : " IPSTR, IP2STR(&(event->ip_info.ip)));

                // Send Notification
                memset(&dq_i, sizeof(util_dataqueue_item_t), 0);
                sprintf(dq_i.data_buff.value.ip, IPSTR, IP2STR(&(event->ip_info.ip)));
                dq_i.data = DRIVER_WIFI_NOTIFICATION_GOT_IP;
                s_notify(&dq_i, 0);
                break;
            
            case IP_EVENT_STA_LOST_IP:
                ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "IP_EVENT_STA_LOST_IP");

                dq_i.data = DRIVER_WIFI_NOTIFICATION_LOST_IP;
                s_notify(&dq_i, 0);
                break;
            
            default:
                break;
        }
        
        return;
    }
}

static void s_event_handler_smartconfig(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    // Smartconfig Event Handler

    util_dataqueue_item_t dq_i;
    dq_i.data_type = DATA_TYPE_NOTIFICATION;

    if(event_base == SC_EVENT)
    {
        switch(event_id)
        {
            case SC_EVENT_SCAN_DONE:
                ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "SC_EVENT_SCAN_DONE");
                break;
            
            case SC_EVENT_FOUND_CHANNEL:
                ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "SC_EVENT_FOUND_CHANNEL");
                break;
            
            case SC_EVENT_GOT_SSID_PSWD:
                ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "SC_EVENT_GOT_SSID_PSWD");

                smartconfig_event_got_ssid_pswd_t *evt = event_data;
                memset(s_ssid, 0, DRIVER_WIFI_LEN_SSID_MAX);
                memset(s_password, 0, DRIVER_WIFI_LEN_PWD_MAX);
                memcpy(s_ssid, evt->ssid, DRIVER_WIFI_LEN_SSID_MAX);
                memcpy(s_password, evt->password, DRIVER_WIFI_LEN_PWD_MAX);

                // Send Notification
                dq_i.data = DRIVER_WIFI_NOTIFICATION_SMARTCONFIG_GOT_CREDENTIALS;
                s_notify(&dq_i, 0);
                break;
            
            case SC_EVENT_SEND_ACK_DONE:
                ESP_LOGI(DEBUG_TAG_DRIVER_WIFI, "SC_EVENT_SEND_ACK_DONE");
                
                // Stop SmartConfig
                esp_smartconfig_stop();
                break;
            
            default:
                break;
        }

        return;
    }
}