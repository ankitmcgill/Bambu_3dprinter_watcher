// DRIVER_CHIPINFO
// AUGUST 26, 2025

#include "driver_chipinfo.h"
#include "define_common_data_types.h"
#include "define_rtos_tasks.h"

// Local Variables
static rtos_component_type_t s_component_type;

// Local Functions

// External Functions
bool DRIVER_CHIPINFO_Init(void)
{
    //Initialize DRIVER_CHIPINFO

    s_component_type = COMPONENT_TYPE_NON_TASK;

    ESP_LOGI(DEBUG_TAG_DRIVER_CHIPINFO, "Type %u. Init", s_component_type);

    return true;
}

bool DRIVER_CHIPINFO_GetChipInfo(esp_chip_info_t *info)
{
    // Return Chip Info

    esp_chip_info(info);
    
    return true;
}

bool DRIVER_CHIPINFO_GetChipID(uint8_t* val)
{
    // Return Unique Chip ID (WiFi Mac Address)

    if(esp_read_mac(val, ESP_MAC_WIFI_STA) != ESP_OK)
    {
        return false;
    }

    return true;
}

uint32_t DRIVER_CHIPINFO_GetFlashSizeBytes(void)
{
    // Return Chip FLASH Size In Bytes

    uint32_t size;

    if(esp_flash_get_size(NULL, &size) != ESP_OK)
    {
        return 0;
    }
    return size;
}

uint32_t DRIVER_CHIPINFO_GetRamSizeBytes(void)
{
    // Return Chip RAM Size In Bytes

    return 0;
}