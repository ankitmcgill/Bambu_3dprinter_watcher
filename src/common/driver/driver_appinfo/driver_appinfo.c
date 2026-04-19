// DRIVER_APPINFO
// AUGUST 26, 2025

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver_appinfo.h"
#include "define_common_data_types.h"
#include "define_rtos_tasks.h"

// Extern Variables
extern const esp_app_desc_t esp_app_desc;

// Local Variables
static rtos_component_type_t s_component_type;

// Local Functions

// External Functions
bool DRIVER_APPINFO_Init(void)
{
    //Initialize DRIVER_CHIPINFO

    s_component_type = COMPONENT_TYPE_NON_TASK;

    ESP_LOGI(DEBUG_TAG_DRIVER_APPINFO, "Type %u. Init", s_component_type);

    return true;
}

bool DRIVER_APPINFO_GetProjectName(char* str)
{
    // Return Project Name

    sprintf(str, "%s", esp_app_desc.project_name);

    return true;
}

bool DRIVER_APPINFO_GetCompileDateTime(char* str)
{
    // Return Compile Date & Time

    sprintf(str, "%s %s", 
        esp_app_desc.time,
        esp_app_desc.date);

    return true;
}

bool DRIVER_APPINFO_GetGitDetails(char* str)
{
    // Return Git Details

    sprintf(str, "Branch : %s | Hash : %s | Tag : %s", 
        GIT_BRANCH,
        GIT_HASH,
        GIT_TAG);

    return true;
}

bool DRIVER_APPINFO_GetIDFVersion(char* str)
{
    // Return IDF Version

    sprintf(str, "%u.%u.%u",
        ESP_IDF_VERSION_MAJOR,
        ESP_IDF_VERSION_MINOR,
        ESP_IDF_VERSION_PATCH
    );

    return true;
}