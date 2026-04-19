#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver_chipinfo.h"
#include "driver_appinfo.h"
#include "driver_wifi.h"
#include "module_wifi.h"
#include "util_dataqueue.h"
#include "define_rtos_tasks.h"
#include "project_defines.h"

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize Chip & App Info
    DRIVER_CHIPINFO_Init();
    DRIVER_APPINFO_Init();

    esp_chip_info_t c_info;
    uint8_t buffer[50] = {0};
    uint32_t size_flash;
    uint32_t size_ram;


    size_flash = DRIVER_CHIPINFO_GetFlashSizeBytes();
    size_ram = DRIVER_CHIPINFO_GetRamSizeBytes();

    // Print App Information
    ESP_LOGI(DEBUG_TAG_MAIN, "-----------------------------------------------");
    memset(buffer, 0, 50);
    DRIVER_APPINFO_GetProjectName((char*)buffer);
    ESP_LOGI(DEBUG_TAG_MAIN, "PROJECT NAME : %s", (char*)buffer);
    memset(buffer, 0, 50);
    DRIVER_APPINFO_GetCompileDateTime((char*)buffer);
    ESP_LOGI(DEBUG_TAG_MAIN, "COMPILE DATETIME : %s", (char*)buffer);
    memset(buffer, 0, 50);
    DRIVER_APPINFO_GetIDFVersion((char*)buffer);
    ESP_LOGI(DEBUG_TAG_MAIN, "IDF VERSION : %s", (char*)buffer);
    memset(buffer, 0, 50);
    DRIVER_APPINFO_GetGitDetails((char*)buffer);
    ESP_LOGI(DEBUG_TAG_MAIN, "GIT DETAILS : %s", (char*)buffer);
    ESP_LOGI(DEBUG_TAG_MAIN, "-----------------------------------------------");

    memset(buffer, 0, 50);
    DRIVER_CHIPINFO_GetChipInfo(&c_info);
    DRIVER_CHIPINFO_GetChipID(buffer);

    // Print Chip Information
    ESP_LOGI(DEBUG_TAG_MAIN, "-----------------------------------------------");
    ESP_LOGI(DEBUG_TAG_MAIN, "MAC : "MACSTR, MAC2STR(buffer));
    ESP_LOGI(DEBUG_TAG_MAIN, "CHIP INFO: %s %s %s %s",
        CONFIG_IDF_TARGET,
        (c_info.features & CHIP_FEATURE_WIFI_BGN) ? "WIFI" : "",
        (c_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
        (c_info.features & CHIP_FEATURE_BT) ? "BT" : ""
    );
    ESP_LOGI(DEBUG_TAG_MAIN, "FLASH : %u MB", size_flash/(1024 * 1024));
    ESP_LOGI(DEBUG_TAG_MAIN, "-----------------------------------------------");

    // Set Internal Module Logging
    esp_log_level_set("wifi", ESP_LOG_NONE);
    esp_log_level_set("esp_netif_handlers", ESP_LOG_NONE);
    esp_log_level_set("wifi_init", ESP_LOG_NONE);
    
    // Main Code Starts
    ESP_LOGI(DEBUG_TAG_MAIN, "");
    ESP_LOGI(DEBUG_TAG_MAIN, "Init");
    ESP_LOGI(DEBUG_TAG_MAIN, "");

    // Intialize Drivers & Modules
    DRIVER_WIFI_Init();
    MODULE_WIFI_Init();

    // Start Scheduler
    // No Need. ESP-IDF Automatically Starts The Scheduler Before main Is Called
    
    // Connect To Wifi
    util_dataqueue_item_t dq_i;
    dq_i.data_type = DATA_TYPE_COMMAND;
    dq_i.data = MODULE_WIFI_COMMAND_CONNECT;
    MODULE_WIFI_AddCommand(&dq_i);

    while(true)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    vTaskDelete(NULL);
}
