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
#include "driver_lcd.h"
#include "driver_wifi.h"
#include "driver_mqtt.h"
#include "driver_nvs.h"
#include "module_wifi.h"
#include "module_mqtt.h"
#include "module_printer.h"
#include "util_dataqueue.h"
#include "define_rtos_tasks.h"
#include "project_defines.h"

// Local Variables
static util_dataqueue_t main_dataqueue;
static util_dataqueue_t printer_dataqueue;
static module_printer_parameters_t s_printer_params;

// Local Functions
static void s_set_screen2_message(const char* format, ...);
static void s_on_printer_data_change(void);

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

    UTIL_DATAQUEUE_Create(&main_dataqueue, 8);
    UTIL_DATAQUEUE_Create(&printer_dataqueue, 8);
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

    // Intialize Display
    DRIVER_LCD_Init();

    // Show Startup Display
    util_dataqueue_item_t dq_i;
    dq_i.data_type = DATA_TYPE_COMMAND;
    dq_i.data = DRIVER_LCD_COMMAND_LOAD_UI_SCREEN;
    dq_i.data_buff.value.uint8 = DRIVER_LCD_SCREEN_STARTUP;
    DRIVER_LCD_AddCommand(&dq_i);

    // Intialize Network
    DRIVER_WIFI_Init();
    MODULE_WIFI_Init();
    // Subscribe To Module Wifi Notifications
    MODULE_WIFI_AddNotificationTarget(&main_dataqueue);

    // Initialize MQTT Stack
    DRIVER_MQTT_Init();
    MODULE_MQTT_Init();
    DRIVER_NVS_Init();

    // Initialize Printer Module
    MODULE_PRINTER_Init();
    // Subscribe To Module Printer Notifications
    MODULE_PRINTER_AddNotificationTarget(&printer_dataqueue);

    // Wifi cycle starts automatically after AP window expires (see module_wifi)

    // Start Scheduler
    // No Need. ESP-IDF Automatically Starts The Scheduler Before main Is Called

    // Turn Of Uneccesary Loggings
    esp_log_level_set("D.Wifi", ESP_LOG_NONE);
    // esp_log_level_set("D.Lcd_Lvgl", ESP_LOG_NONE);

    while(true)
    {
        if(UTIL_DATAQUEUE_MessageCheck(&main_dataqueue))
        {
            if(UTIL_DATAQUEUE_MessageGet(&main_dataqueue, &dq_i, 0))
            {
                if(dq_i.data_type == DATA_TYPE_NOTIFICATION)
                {
                    switch(dq_i.data)
                    {
                        case MODULE_WIFI_NOTIFICATION_APSTARTED:
                            s_set_screen2_message("Setup Portal\nActive\nFor %u sec", MODULE_WIFI_AP_WINDOW_SEC);
                            ESP_LOGI(DEBUG_TAG_MAIN, "WiFi: AP Started");
                            break;

                        case MODULE_WIFI_NOTIFICATION_CHECKING_SAVED_CREDENTIALS:
                            s_set_screen2_message("Checking Saved\nWiFi\nCredentials");
                            ESP_LOGI(DEBUG_TAG_MAIN, "WiFi: Checking Saved Credentials");
                            break;

                        case MODULE_WIFI_NOTIFICATION_CHECKING_DEFAULT_CREDENTIALS:
                            s_set_screen2_message("Checking Default\nWiFi\nCredentials");
                            ESP_LOGI(DEBUG_TAG_MAIN, "WiFi: Checking Default Credentials");
                            break;

                        case MODULE_WIFI_NOTIFICATION_CONNECTED:
                            s_set_screen2_message("Connected To\nWiFi");
                            ESP_LOGI(DEBUG_TAG_MAIN, "WiFi: Connected");
                            break;

                        case MODULE_WIFI_NOTIFICATION_GOT_IP:
                            s_set_screen2_message("Got IP\n%s", dq_i.data_buff.value.ip);
                            ESP_LOGI(DEBUG_TAG_MAIN, "WiFi: Got IP %s", dq_i.data_buff.value.ip);

                            // Connect To Mqtt Broker & Topic
                            MODULE_PRINTER_Connect();

                            // Switch To Screen 1
                            // After 4 Second Delay
                            vTaskDelay(pdMS_TO_TICKS(2000));
                            dq_i.data_type = DATA_TYPE_COMMAND;
                            dq_i.data = DRIVER_LCD_COMMAND_LOAD_UI_SCREEN;
                            dq_i.data_buff.value.uint8 = DRIVER_LCD_SCREEN_HOME;
                            DRIVER_LCD_AddCommand(&dq_i);
                            break;

                        default:
                            break;
                    }
                }
            }
        }

        if(UTIL_DATAQUEUE_MessageCheck(&printer_dataqueue))
        {
            if(UTIL_DATAQUEUE_MessageGet(&printer_dataqueue, &dq_i, 0))
            {
                if(dq_i.data_type == DATA_TYPE_NOTIFICATION)
                {
                    switch(dq_i.data)
                    {
                        case MODULE_PRINTER_NOTIFICATION_DATA_CHANGE: {
                            module_printer_parameters_t* src = (module_printer_parameters_t*)dq_i.data_buff.value.ptr;
                            if(!src) break;
                            if(src->is_dirty_state){
                                s_printer_params.state = src->state;
                                s_printer_params.is_dirty_state = true;
                                src->is_dirty_state = false;
                            }
                            if(src->is_dirty_gcode_status){
                                s_printer_params.gcode_status = src->gcode_status;
                                s_printer_params.is_dirty_gcode_status = true;
                                src->is_dirty_gcode_status = false;
                            }
                            if(src->is_dirty_nozzle_temp){
                                s_printer_params.nozzle_temp = src->nozzle_temp;
                                s_printer_params.is_dirty_nozzle_temp = true;
                                src->is_dirty_nozzle_temp = false;
                            }
                            if(src->is_dirty_nozzle_temp_target){
                                s_printer_params.nozzle_temp_target = src->nozzle_temp_target;
                                s_printer_params.is_dirty_nozzle_temp_target = true;
                                src->is_dirty_nozzle_temp_target = false;
                            }
                            if(src->is_dirty_bed_temp){
                                s_printer_params.bed_temp = src->bed_temp;
                                s_printer_params.is_dirty_bed_temp = true;
                                src->is_dirty_bed_temp = false;
                            }
                            if(src->is_dirty_bed_temp_target){
                                s_printer_params.bed_temp_target = src->bed_temp_target;
                                s_printer_params.is_dirty_bed_temp_target = true;
                                src->is_dirty_bed_temp_target = false;
                            }
                            //
                            if(src->is_dirty_print_progress_percentage){
                                s_printer_params.print_progress_percentage = src->print_progress_percentage;
                                s_printer_params.is_dirty_print_progress_percentage = true;
                                src->is_dirty_print_progress_percentage = false;
                            }
                            if(src->is_dirty_current_layer){
                                s_printer_params.current_layer = src->current_layer;
                                s_printer_params.is_dirty_current_layer = true;
                                src->is_dirty_current_layer = false;
                            }
                            if(src->is_dirty_total_layer){
                                s_printer_params.total_layer = src->total_layer;
                                s_printer_params.is_dirty_total_layer = true;
                                src->is_dirty_total_layer = false;
                            }
                            if(src->is_dirty_time_remaining_s){
                                s_printer_params.time_remaining_s = src->time_remaining_s;
                                s_printer_params.is_dirty_time_remaining_s = true;
                                src->is_dirty_time_remaining_s = false;
                            }
                            s_on_printer_data_change();
                            break;
                        }

                        default:
                            break;
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    vTaskDelete(NULL);
}

static void s_set_screen2_message(const char* format, ...)
{
    // Set Screen2 Message

    util_dataqueue_item_t dq_i;
    char message[64];
    va_list arglist;

    va_start(arglist, format);
    vsprintf(message, format, arglist);
    va_end(arglist);

    dq_i.data_type = DATA_TYPE_COMMAND;
    dq_i.data = DRIVER_LCD_COMMAND_SET_SCREEN2_MESSAGE;
    strncpy(dq_i.data_buff.value.msg, message, sizeof(dq_i.data_buff.value.msg) - 1);
    DRIVER_LCD_AddCommand(&dq_i);
}

static void s_on_printer_data_change(void)
{
    // Send Driver LCD Commands For All Dirty Printer Parameters

    util_dataqueue_item_t dq_i;
    char buf[16];

    if(s_printer_params.is_dirty_state){
        module_printer_state_t state = s_printer_params.state;

        dq_i.data_type = DATA_TYPE_COMMAND;
        dq_i.data = DRIVER_LCD_COMMAND_SET_SCREEN1_MESSAGE_STATUS;
        strncpy(dq_i.data_buff.value.msg,
                (state == MODULE_PRINTER_STATE_ONLINE) ? "ONLINE" : "OFFLINE",
                sizeof(dq_i.data_buff.value.msg) - 1);
        DRIVER_LCD_AddCommand(&dq_i);

        dq_i.data_type = DATA_TYPE_COMMAND;
        dq_i.data = DRIVER_LCD_COMMAND_SET_SCREEN1_PANEL_STATUS_COLOR;
        dq_i.data_buff.value.uint8 = (state == MODULE_PRINTER_STATE_ONLINE)
            ? DRIVER_LCD_STATUS_COLOR_GREEN
            : DRIVER_LCD_STATUS_COLOR_RED;
        DRIVER_LCD_AddCommand(&dq_i);

        dq_i.data_type = DATA_TYPE_COMMAND;
        dq_i.data = DRIVER_LCD_COMMAND_SET_SCREEN1_PRINTERDATA_VISIBLE;
        dq_i.data_buff.value.uint8 = (state == MODULE_PRINTER_STATE_ONLINE) ? 1 : 0;
        DRIVER_LCD_AddCommand(&dq_i);

        ESP_LOGI(DEBUG_TAG_MAIN, "Printer state: %s", (state == MODULE_PRINTER_STATE_ONLINE) ? "Online" : "Offline");
        s_printer_params.is_dirty_state = false;
    }

    if(s_printer_params.is_dirty_gcode_status){
        const char* mode_str;
        switch(s_printer_params.gcode_status){
            case MODULE_PRINTER_GCODE_STATUS_IDLE:    mode_str = "IDLE";    break;
            case MODULE_PRINTER_GCODE_STATUS_PREPARE: mode_str = "PREPARE"; break;
            case MODULE_PRINTER_GCODE_STATUS_RUNNING: mode_str = "RUNNING"; break;
            case MODULE_PRINTER_GCODE_STATUS_PAUSE:   mode_str = "PAUSE";   break;
            case MODULE_PRINTER_GCODE_STATUS_FINISH:  mode_str = "FINISH";  break;
            case MODULE_PRINTER_GCODE_STATUS_FAILED:  mode_str = "FAILED";  break;
            default:                                   mode_str = "";        break;
        }
        dq_i.data_type = DATA_TYPE_COMMAND;
        dq_i.data = DRIVER_LCD_COMMAND_SET_SCREEN1_LABEL_MODE;
        strncpy(dq_i.data_buff.value.msg, mode_str, sizeof(dq_i.data_buff.value.msg) - 1);
        DRIVER_LCD_AddCommand(&dq_i);

        ESP_LOGI(DEBUG_TAG_MAIN, "Printer gcode: %s", mode_str);
        s_printer_params.is_dirty_gcode_status = false;
    }

    if(s_printer_params.is_dirty_nozzle_temp){
        snprintf(buf, sizeof(buf), "%u ""\xB0" "C", s_printer_params.nozzle_temp);
        dq_i.data_type = DATA_TYPE_COMMAND;
        dq_i.data = DRIVER_LCD_COMMAND_SET_SCREEN1_LABEL_NOZZLE_TEMP_ACTUAL;
        strncpy(dq_i.data_buff.value.msg, buf, sizeof(dq_i.data_buff.value.msg) - 1);
        DRIVER_LCD_AddCommand(&dq_i);
        s_printer_params.is_dirty_nozzle_temp = false;
    }

    if(s_printer_params.is_dirty_nozzle_temp_target){
        snprintf(buf, sizeof(buf), "%u ""\xB0" "C", s_printer_params.nozzle_temp_target);
        dq_i.data_type = DATA_TYPE_COMMAND;
        dq_i.data = DRIVER_LCD_COMMAND_SET_SCREEN1_LABEL_NOZZLE_TEMP_TARGET;
        strncpy(dq_i.data_buff.value.msg, buf, sizeof(dq_i.data_buff.value.msg) - 1);
        DRIVER_LCD_AddCommand(&dq_i);
        s_printer_params.is_dirty_nozzle_temp_target = false;
    }

    if(s_printer_params.is_dirty_bed_temp){
        snprintf(buf, sizeof(buf), "%u ""\xB0" "C", s_printer_params.bed_temp);
        dq_i.data_type = DATA_TYPE_COMMAND;
        dq_i.data = DRIVER_LCD_COMMAND_SET_SCREEN1_LABEL_BED_TEMP_ACTUAL;
        strncpy(dq_i.data_buff.value.msg, buf, sizeof(dq_i.data_buff.value.msg) - 1);
        DRIVER_LCD_AddCommand(&dq_i);
        s_printer_params.is_dirty_bed_temp = false;
    }

    if(s_printer_params.is_dirty_bed_temp_target){
        snprintf(buf, sizeof(buf), "%u ""\xB0" "C", s_printer_params.bed_temp_target);
        dq_i.data_type = DATA_TYPE_COMMAND;
        dq_i.data = DRIVER_LCD_COMMAND_SET_SCREEN1_LABEL_BED_TEMP_TARGET;
        strncpy(dq_i.data_buff.value.msg, buf, sizeof(dq_i.data_buff.value.msg) - 1);
        DRIVER_LCD_AddCommand(&dq_i);
        s_printer_params.is_dirty_bed_temp_target = false;
    }

    if(s_printer_params.is_dirty_print_progress_percentage){
        dq_i.data_type = DATA_TYPE_COMMAND;
        dq_i.data = DRIVER_LCD_COMMAND_SET_SCREEN1_PROGRESS;
        dq_i.data_buff.value.uint8 = s_printer_params.print_progress_percentage;
        DRIVER_LCD_AddCommand(&dq_i);
        s_printer_params.is_dirty_print_progress_percentage = false;
    }

    if(s_printer_params.is_dirty_current_layer){
        snprintf(buf, sizeof(buf), "%u", s_printer_params.current_layer);
        dq_i.data_type = DATA_TYPE_COMMAND;
        dq_i.data = DRIVER_LCD_COMMAND_SET_SCREEN1_LABEL_LAYER_ACTUAL;
        strncpy(dq_i.data_buff.value.msg, buf, sizeof(dq_i.data_buff.value.msg) - 1);
        DRIVER_LCD_AddCommand(&dq_i);
        s_printer_params.is_dirty_current_layer = false;
    }

    if(s_printer_params.is_dirty_total_layer){
        snprintf(buf, sizeof(buf), "%u", s_printer_params.total_layer);
        dq_i.data_type = DATA_TYPE_COMMAND;
        dq_i.data = DRIVER_LCD_COMMAND_SET_SCREEN1_LABEL_LAYER_TARGET;
        strncpy(dq_i.data_buff.value.msg, buf, sizeof(dq_i.data_buff.value.msg) - 1);
        DRIVER_LCD_AddCommand(&dq_i);
        s_printer_params.is_dirty_total_layer = false;
    }

    if(s_printer_params.is_dirty_time_remaining_s){
        uint32_t t = s_printer_params.time_remaining_s;
        snprintf(dq_i.data_buff.value.msg, sizeof(dq_i.data_buff.value.msg),
                 "%02"PRIu32" : %02"PRIu32" : %02"PRIu32,
                 t / 3600, (t % 3600) / 60, t % 60);
        dq_i.data_type = DATA_TYPE_COMMAND;
        dq_i.data = DRIVER_LCD_COMMAND_SET_SCREEN1_LABEL_TIME_REMAINING;
        DRIVER_LCD_AddCommand(&dq_i);
        s_printer_params.is_dirty_time_remaining_s = false;
    }
}
