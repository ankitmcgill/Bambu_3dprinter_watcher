// DRIVER_NVS
// APRIL 29, 2026

#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "driver_nvs.h"
#include "define_common_data_types.h"
#include "define_rtos_tasks.h"

// NVS
#define NVS_NAMESPACE       "bambu_cfg"
#define NVS_KEY_API_KEY     "api_key"
#define NVS_KEY_USER_ID     "user_id"
#define NVS_KEY_DEVICE_ID   "device_id"
#define NVS_KEY_REGION      "region"

// Local Variables
static rtos_component_type_t s_component_type;
static driver_nvs_config_t s_config;

// Local Functions
static void s_nvs_read_str(nvs_handle_t h, const char* key, char* out, size_t out_len);

// External Functions
bool DRIVER_NVS_Init(void)
{
    // Initialize Driver Nvs

    s_component_type = COMPONENT_TYPE_NON_TASK;

    memset(&s_config, 0, sizeof(s_config));

    ESP_LOGI(DEBUG_TAG_DRIVER_NVS, "Type %u. Init", s_component_type);

    return true;
}

driver_nvs_config_t* DRIVER_NVS_ReadConfig(void)
{
    // Read Config Fields From NVS And Return Pointer To Static Struct

    nvs_handle_t h;

    if(nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK){
        ESP_LOGW(DEBUG_TAG_DRIVER_NVS, "NVS open failed");
        return NULL;
    }

    memset(&s_config, 0, sizeof(s_config));

    s_nvs_read_str(h, NVS_KEY_API_KEY,   s_config.api_key,   sizeof(s_config.api_key));
    s_nvs_read_str(h, NVS_KEY_USER_ID,   s_config.user_id,   sizeof(s_config.user_id));
    s_nvs_read_str(h, NVS_KEY_DEVICE_ID, s_config.device_id, sizeof(s_config.device_id));
    s_nvs_read_str(h, NVS_KEY_REGION,    s_config.region,    sizeof(s_config.region));

    nvs_close(h);

    ESP_LOGI(DEBUG_TAG_DRIVER_NVS, "Config Read. user_id=%s device_id=%s region=%s",
        s_config.user_id, s_config.device_id, s_config.region);

    return &s_config;
}

static void s_nvs_read_str(nvs_handle_t h, const char* key, char* out, size_t out_len)
{
    // Read String From NVS Into Out Buffer; Leaves out[0]='\0' On Miss Or Error

    size_t len = out_len;
    if(nvs_get_str(h, key, out, &len) != ESP_OK){
        out[0] = '\0';
    }
}
