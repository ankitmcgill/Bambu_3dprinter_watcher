// DRIVER_SPIFFS
// OCTOBER 3, 2025

#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "driver_spiffs.h"
#include "define_common_data_types.h"
#include "define_rtos_tasks.h"

// Extern Variables

// Local Variables
static rtos_component_type_t s_component_type;

// Local Functions

// External Functions
bool DRIVER_SPIFFS_Init(void)
{
    // Initialize Driver Spiffs

    esp_err_t ret;
    s_component_type = COMPONENT_TYPE_NON_TASK;
    esp_vfs_spiffs_conf_t config_spiffs = {
        .base_path = DRIVER_SPIFFS_MOUNT_PATH,
        .partition_label = NULL,
        .max_files = DRIVER_SPIFFS_FILES_MAX,
        .format_if_mount_failed = true
    };

    // Initialize & Mount Spiffs File System
    ret = esp_vfs_spiffs_register(&config_spiffs);

    if(ret != ESP_OK)
    {
        switch(ret)
        {
            case ESP_FAIL:
                ESP_LOGE(DEBUG_TAG_DRIVER_SPIFFS, "FS Mount Or Format Failed!");
                return false;
                break;
            
            case ESP_ERR_NOT_FOUND:
                ESP_LOGE(DEBUG_TAG_DRIVER_SPIFFS, "Failed To Find Spiffs Partition!");
                return false;
                break;

            default:
                ESP_LOGE(DEBUG_TAG_DRIVER_SPIFFS, "Failed To Init - (%)!", esp_err_to_name(ret));
                return false;
                break;
        }
    }
    
    ESP_LOGI(DEBUG_TAG_DRIVER_SPIFFS, "Type %u. Init", s_component_type);
    
    return true;
}

bool DRIVER_SPIFFS_ReadFile(char* file_path, char* buffer)
{
    //Spiffs Read File & Return Contents In Buffer

    int ch;
    FILE* fp; 
    
    fp = fopen(file_path, "r");
    if(fp == NULL){
        return false;
    }

    while((ch = fgetc(fp)) != EOF){
        *buffer = (char)ch;
        buffer += 1;
    }
    buffer = '\0';
    fclose(fp);
    
    return true;
}

void DRIVER_SPIFFS_PrintInfo(void)
{
    //Print Spiffs Fs Info

    size_t total_bytes;
    size_t used_bytes;
    uint8_t file_count;
    esp_err_t ret;

    ret = esp_spiffs_info(NULL, &total_bytes, &used_bytes);

    file_count = 0;
    DIR* dir = opendir(DRIVER_SPIFFS_MOUNT_PATH);
    if(dir){
        struct dirent* entry;

        while((entry = readdir(dir)) != NULL){
            file_count += 1;
        }
        closedir(dir);
    }

    if(ret == ESP_OK){
        ESP_LOGI(DEBUG_TAG_DRIVER_SPIFFS, "-----------------------------------------------");
        ESP_LOGI(DEBUG_TAG_DRIVER_SPIFFS, "TOTAL BYTES : %u", total_bytes);
        ESP_LOGI(DEBUG_TAG_DRIVER_SPIFFS, "USED BYTES : %u", used_bytes);
        ESP_LOGI(DEBUG_TAG_DRIVER_SPIFFS, "FREE BYTES : %u", total_bytes - used_bytes);
        ESP_LOGI(DEBUG_TAG_DRIVER_SPIFFS, "FILE COUNT : %u", file_count);
        ESP_LOGI(DEBUG_TAG_DRIVER_SPIFFS, "-----------------------------------------------");

        return;
    }

    ESP_LOGE(DEBUG_TAG_DRIVER_SPIFFS, "Print Info Failed - %s", esp_err_to_name(ret));
}