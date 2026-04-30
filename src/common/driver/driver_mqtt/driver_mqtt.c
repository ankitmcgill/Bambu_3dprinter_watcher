// DRIVER_MQTT
// APRIL 28, 2026

#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_crt_bundle.h"

#include "driver_mqtt.h"
#include "util_dataqueue.h"
#include "define_common_data_types.h"
#include "define_rtos_tasks.h"

// Extern Variables
TaskHandle_t handle_task_driver_mqtt;

// Local Variables
static rtos_component_type_t s_component_type;
static util_dataqueue_t s_dataqueue;
static uint8_t s_notification_targets_count;
static util_dataqueue_t* s_notification_targets[DRIVER_MQTT_NOTIFICATION_TARGET_MAX];
static esp_mqtt_client_handle_t s_mqtt_client;
static char s_broker_url[DRIVER_MQTT_URL_LEN_MAX];
static char s_topic[DRIVER_MQTT_TOPIC_LEN_MAX];
static char s_username[DRIVER_MQTT_USERNAME_LEN_MAX];
static char s_password[DRIVER_MQTT_PASSWORD_LEN_MAX];

// Local Functions
static bool s_notify(util_dataqueue_item_t* dq_i, TickType_t wait);
static void s_task_function(void *pvParameters);

// Callbacks
static void s_event_handler_mqtt(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// External Functions
bool DRIVER_MQTT_Init(void)
{
    // Initialize Driver Mqtt

    s_component_type = COMPONENT_TYPE_TASK;

    ESP_LOGI(DEBUG_TAG_DRIVER_MQTT, "Type %u. Init", s_component_type);

    // Create Data Queue
    UTIL_DATAQUEUE_Create(&s_dataqueue, DRIVER_MQTT_DATAQUEUE_MAX);
    s_notification_targets_count = 0;

    memset(s_broker_url, 0, sizeof(s_broker_url));
    memset(s_topic,      0, sizeof(s_topic));
    memset(s_username,   0, sizeof(s_username));
    memset(s_password,   0, sizeof(s_password));

    // Create Driver Mqtt Task
    xTaskCreate(
        s_task_function,
        "t-d-mqtt",
        TASK_STACK_DEPTH_DRIVER_MQTT,
        NULL,
        TASK_PRIORITY_DRIVER_MQTT,
        &handle_task_driver_mqtt
    );

    return true;
}

void DRIVER_MQTT_SetCredentials(const char* username, const char* password)
{
    // Set Mqtt Credentials

    strncpy(s_username, username, DRIVER_MQTT_USERNAME_LEN_MAX - 1);
    strncpy(s_password, password, DRIVER_MQTT_PASSWORD_LEN_MAX - 1);

    ESP_LOGI(DEBUG_TAG_DRIVER_MQTT, "Credentials Set. Username: %s", s_username);
    ESP_LOGI(DEBUG_TAG_DRIVER_MQTT, "Credentials Set. Password: %s", s_password);
}

void DRIVER_MQTT_SetBrokerUrl(const char* url)
{
    // Set Mqtt Broker Url

    strncpy(s_broker_url, url, DRIVER_MQTT_URL_LEN_MAX - 1);

    ESP_LOGI(DEBUG_TAG_DRIVER_MQTT, "Broker Url Set: %s", s_broker_url);
}

void DRIVER_MQTT_SetTopic(const char* topic)
{
    // Set Mqtt Subscribe Topic

    strncpy(s_topic, topic, DRIVER_MQTT_TOPIC_LEN_MAX - 1);

    ESP_LOGI(DEBUG_TAG_DRIVER_MQTT, "Topic Set: %s", s_topic);
}

bool DRIVER_MQTT_AddCommand(util_dataqueue_item_t* dq_i)
{
    // Add Command

    if(!UTIL_DATAQUEUE_MessageQueue(&s_dataqueue, dq_i, 0)){
        ESP_LOGW(DEBUG_TAG_DRIVER_MQTT, "Message Queue Failed %s", __FILE__);

        return false;
    }

    return true;
}

bool DRIVER_MQTT_AddNotificationTarget(util_dataqueue_t* dq)
{
    // Add Notification Target

    if(s_notification_targets_count >= DRIVER_MQTT_NOTIFICATION_TARGET_MAX){
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
            ESP_LOGW(DEBUG_TAG_DRIVER_MQTT, "Message Queue Failed %s", __FILE__);
        }
    }

    return true;
}

static void s_task_function(void *pvParameters)
{
    // Task Function

    util_dataqueue_item_t dq_i;

    ESP_LOGI(DEBUG_TAG_DRIVER_MQTT, "Starting task");

    while(true){
        // Check Data Queue
        if(UTIL_DATAQUEUE_MessageCheck(&s_dataqueue))
        {
            if(UTIL_DATAQUEUE_MessageGet(&s_dataqueue, &dq_i, 0))
            {
                ESP_LOGI(DEBUG_TAG_DRIVER_MQTT, "New In DataQueue. Type %u, Data %u", dq_i.data_type, dq_i.data);

                if(dq_i.data_type == DATA_TYPE_COMMAND)
                {
                    switch(dq_i.data)
                    {
                        case DRIVER_MQTT_COMMAND_CONNECT: {
                            if(s_mqtt_client != NULL){
                                esp_mqtt_client_stop(s_mqtt_client);
                                esp_mqtt_client_destroy(s_mqtt_client);
                                s_mqtt_client = NULL;
                            }

                            esp_mqtt_client_config_t mqtt_cfg = {
                                .broker = {
                                    .address.uri          = s_broker_url,
                                    .verification.crt_bundle_attach = esp_crt_bundle_attach,
                                },
                                .credentials = {
                                    .username = s_username,
                                    .authentication.password = s_password,
                                },
                                .buffer = {
                                    .size = 4096,
                                },
                            };

                            s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
                            assert(s_mqtt_client);

                            esp_mqtt_client_register_event(
                                s_mqtt_client,
                                ESP_EVENT_ANY_ID,
                                s_event_handler_mqtt,
                                NULL
                            );

                            ESP_LOGI(DEBUG_TAG_DRIVER_MQTT, "Connecting to %s", s_broker_url);
                            esp_mqtt_client_start(s_mqtt_client);
                            break;
                        }

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

static void s_event_handler_mqtt(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    // Mqtt Event Handler

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    util_dataqueue_item_t dq_i;
    dq_i.data_type = DATA_TYPE_NOTIFICATION;

    switch((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(DEBUG_TAG_DRIVER_MQTT, "MQTT_EVENT_CONNECTED");

            esp_mqtt_client_subscribe(s_mqtt_client, s_topic, 0);
            ESP_LOGI(DEBUG_TAG_DRIVER_MQTT, "Subscribed to %s", s_topic);

            dq_i.data = DRIVER_MQTT_NOTIFICATION_CONNECTED;
            s_notify(&dq_i, 0);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(DEBUG_TAG_DRIVER_MQTT, "MQTT_EVENT_DISCONNECTED");

            dq_i.data = DRIVER_MQTT_NOTIFICATION_DISCONNECTED;
            s_notify(&dq_i, 0);
            break;

        case MQTT_EVENT_DATA: {
            if(event->data_len <= 0) break;

            char* buf = malloc(event->data_len + 1);
            if(!buf){
                ESP_LOGW(DEBUG_TAG_DRIVER_MQTT, "MQTT_EVENT_DATA malloc failed");
                break;
            }
            memcpy(buf, event->data, event->data_len);
            buf[event->data_len] = '\0';

            ESP_LOGI(DEBUG_TAG_DRIVER_MQTT, "MQTT_EVENT_DATA (%d bytes)", event->data_len);

            dq_i.data_buff.value.mqtt_data = buf;
            dq_i.data = DRIVER_MQTT_NOTIFICATION_DATA_RECEIVED;
            if(!s_notify(&dq_i, 0)){
                free(buf);
            }
            break;
        }

        case MQTT_EVENT_ERROR:
            ESP_LOGW(DEBUG_TAG_DRIVER_MQTT, "MQTT_EVENT_ERROR");
            break;

        default:
            break;
    }
}
