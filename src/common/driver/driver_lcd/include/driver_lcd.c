// DRIVER_LCD
// SEPTEMBER 30, 2025

#include <sys/lock.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_idf_version.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "lvgl.h"
#include "display/lv_display.h" 
#include "lv_demos.h"

#include "driver_lcd.h"
#include "util_dataqueue.h"
#include "define_common_data_types.h"
#include "define_rtos_tasks.h"
#include "bsp.h"

#ifdef CONFIG_INCLUDE_UI
#include "ui.h"
#endif

// Display & Frambeuffer Flags
// Lvgl Refresh Modes (DRIVER_LCD_LVGL_USE_FULL_REFRESH or DRIVER_LCD_LVGL_USE_PARTIAL_REFRESH)
#define DRIVER_LCD_LVGL_USE_PARTIAL_REFRESH
// Bounce Buffer
#define DRIVER_LCD_USE_BOUNCE_BUFFER

// Extern Variables

// Local Variables
static TaskHandle_t s_handle_task_lvgl;
static rtos_component_type_t s_component_type;
static util_dataqueue_t s_dataqueue;
static esp_lcd_panel_handle_t s_handle_rgb_panel;
static SemaphoreHandle_t s_handle_semaphore_vsync;
static SemaphoreHandle_t s_handle_semaphore_guiready;
static esp_timer_handle_t s_timer;
static esp_timer_handle_t s_timer_one_second;
static lv_display_t* s_lvgl_display;


// Hacky Code For Second Indicator
static char s_second_panel_visible = true;
static bool s_update_seconds = false;

// Local Functions
static bool s_lcd_rgb_panel_setup(void);
static bool s_lvgl_setup(void);
static void s_task_lvgl(void *arg);
static void s_timer_one_second_cb(void *arg);
static void s_lvgl_tick_timer_cb(void* arg);
static bool s_lcd_rgb_panel_vsync_cb(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event_data, void *user_data);
static bool s_lcd_rgb_panel_color_trans_cb(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx);
static void s_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

// External Functions
bool DRIVER_LCD_Init(void)
{
    // Initialize Driver Lcd

    s_component_type = COMPONENT_TYPE_TASK;

    ESP_LOGI(DEBUG_TAG_DRIVER_LCD, "Type %u. Init", s_component_type);

    s_handle_semaphore_vsync = xSemaphoreCreateBinary();
    assert(s_handle_semaphore_vsync);
    s_handle_semaphore_guiready = xSemaphoreCreateBinary();
    assert(s_handle_semaphore_guiready);

    // Create Data Queue
    UTIL_DATAQUEUE_Create(&s_dataqueue, DRIVER_LCD_DATAQUEUE_MAX);
    assert(s_dataqueue.handle);

    // Create Timer
    const esp_timer_create_args_t timer_args = {
        .callback = &s_timer_one_second_cb,
        .name = "periodic_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_timer_one_second));

    // Initialize Display Subsystems
    if(!s_lcd_rgb_panel_setup()) goto err;
    if(!s_lvgl_setup()) goto err;

    return true;
    err:
        return false;
}

bool DRIVER_LCD_AddCommand(util_dataqueue_item_t* dq_i)
{
    // Add Command

    if(!UTIL_DATAQUEUE_MessageQueue(&s_dataqueue, dq_i, 0)){
        ESP_LOGW(DEBUG_TAG_DRIVER_LCD, "Message Queue Failed %s", __FILE__);

        return false;
    }
    
    return true;
}

static bool s_lcd_rgb_panel_setup(void)
{
    // Initialize LCD Panel & RGB

    esp_err_t ret = ESP_OK;
    s_handle_rgb_panel = NULL;
    const esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_color_trans_done = s_lcd_rgb_panel_color_trans_cb,
        .on_vsync = s_lcd_rgb_panel_vsync_cb
    };
    esp_lcd_rgb_panel_config_t rgb_panel_config = {
        .clk_src = LCD_CLK_SRC_PLL160M,
        .data_width = 16,
        .bits_per_pixel = 16,
        #if defined DRIVER_LCD_LVGL_USE_FULL_REFRESH
        .num_fbs = 2,
        #endif
        #if defined DRIVER_LCD_LVGL_USE_PARTIAL_REFRESH
        .num_fbs = 1,
        #endif
        .psram_trans_align = 64,
        #if defined DRIVER_LCD_USE_BOUNCE_BUFFER
        .bounce_buffer_size_px = 10 * DRIVER_LCD_DISPLAY_HRES,
        #endif
        #if defined DRIVER_LCD_LVGL_USE_FULL_REFRESH
        .flags.fb_in_psram = true,
        #endif
        #if defined DRIVER_LCD_LVGL_USE_PARTIAL_REFRESH
        .flags.fb_in_psram = true,
        #endif
        .timings = {
            .pclk_hz = (18 * 1000 * 1000),
            .h_res = DRIVER_LCD_DISPLAY_HRES,
            .v_res = DRIVER_LCD_DISPLAY_VRES,
            .hsync_pulse_width = 40,
            .hsync_back_porch = 40,
            .hsync_front_porch = 48,
            .vsync_pulse_width = 23,
            .vsync_back_porch = 32,
            .vsync_front_porch = 13,
            .flags.pclk_active_neg = true
        },
        .hsync_gpio_num = BSP_LCD_GPIO_HSYNC,
        .vsync_gpio_num = BSP_LCD_GPIO_VSYNC,
        .pclk_gpio_num = BSP_LCD_GPIO_PCLK,
        .de_gpio_num = BSP_LCD_GPIO_DE,
        .disp_gpio_num = BSP_LCD_GPIO_DISP,
        .data_gpio_nums={
            BSP_LCD_GPIO_DATA0,
            BSP_LCD_GPIO_DATA1,
            BSP_LCD_GPIO_DATA2,
            BSP_LCD_GPIO_DATA3,
            BSP_LCD_GPIO_DATA4,
            BSP_LCD_GPIO_DATA5,
            BSP_LCD_GPIO_DATA6,
            BSP_LCD_GPIO_DATA7,
            BSP_LCD_GPIO_DATA8,
            BSP_LCD_GPIO_DATA9,
            BSP_LCD_GPIO_DATA10,
            BSP_LCD_GPIO_DATA11,
            BSP_LCD_GPIO_DATA12,
            BSP_LCD_GPIO_DATA13,
            BSP_LCD_GPIO_DATA14,
            BSP_LCD_GPIO_DATA15
        }
    };

    (void)ret;
    // Create Esp Lcd Panel
    ESP_GOTO_ON_ERROR(esp_lcd_new_rgb_panel(&rgb_panel_config, &s_handle_rgb_panel),
        err,
        DEBUG_TAG_DRIVER_LCD,
        "Esp_lcd Panel Create Fail"
    );

    // Register Panel Event Callbacks
    ESP_GOTO_ON_ERROR(esp_lcd_rgb_panel_register_event_callbacks(s_handle_rgb_panel, &cbs, (void*)s_lvgl_display),
        err,
        DEBUG_TAG_DRIVER_LCD,
        "ESP_lcd Panel Cb Register Fail"
    );

    // Reset Panel
    ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(s_handle_rgb_panel),
        err,
        DEBUG_TAG_DRIVER_LCD,
        "Lcd Rgb Panel Reset Fail"
    );

    ESP_GOTO_ON_ERROR(
        esp_lcd_panel_init(s_handle_rgb_panel),
        err,
        DEBUG_TAG_DRIVER_LCD,
        "Esp_lcd Panel Reset Fail"
    );

    // Init Panel
    ESP_GOTO_ON_ERROR(esp_lcd_panel_init(s_handle_rgb_panel),
        err,
        DEBUG_TAG_DRIVER_LCD,
        "Esp_lcd Panel Init Fail"
    );

    ESP_LOGI(DEBUG_TAG_DRIVER_LCD, "Lcd Panel Setup Done");
    return true;

    err:
        return false;
}

static bool s_lvgl_setup(void)
{
    // Initialize Lvgl Port

    esp_err_t ret = ESP_OK;
    void* buf1 = NULL;
    void* buf2 = NULL;
    #if defined DRIVER_LCD_LVGL_USE_FULL_REFRESH
    size_t buf_size = (DRIVER_LCD_DISPLAY_HRES * DRIVER_LCD_DISPLAY_VRES * sizeof(lv_color16_t));
    #endif
    #if defined DRIVER_LCD_LVGL_USE_PARTIAL_REFRESH
    size_t buf_size = (100 * DRIVER_LCD_DISPLAY_HRES);
    #endif

    (void)ret;

    // Initialize Lvgl
    lv_init();

    // Allocate Buffers
    #if defined DRIVER_LCD_LVGL_USE_FULL_REFRESH
    // Use Esp Lcd Rgb Panel Buffers
    ESP_LOGI(DEBUG_TAG_DRIVER_LCD, "Using Esp Lcd Rgb Panel fb");
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(s_handle_rgb_panel, 2, &buf1, &buf2));
    assert(buf1 && buf2);
    #endif

    #if defined DRIVER_LCD_LVGL_USE_PARTIAL_REFRESH
    buf1 = heap_caps_malloc(100 * DRIVER_LCD_DISPLAY_HRES * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    assert(buf1);
    buf2 = heap_caps_malloc(100 * DRIVER_LCD_DISPLAY_HRES * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    assert(buf2);
    #endif

    // Create An Lvgl Display & Initialize Buffers
    s_lvgl_display = lv_display_create(DRIVER_LCD_DISPLAY_HRES, DRIVER_LCD_DISPLAY_VRES);
    #if defined DRIVER_LCD_LVGL_USE_FULL_REFRESH
    lv_display_set_buffers(s_lvgl_display, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_FULL);
    #endif
    #if defined DRIVER_LCD_LVGL_USE_PARTIAL_REFRESH
    lv_display_set_buffers(s_lvgl_display, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    #endif
    assert(s_lvgl_display);

    // Set Color Depth
    // Set Display Rotation
    // Set Cb Function That Copies Rendered Image To Display Area
    lv_display_set_color_format(s_lvgl_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_rotation(s_lvgl_display, LV_DISPLAY_ROTATION_180);
    lv_display_set_flush_cb(s_lvgl_display, s_lvgl_flush_cb);

    ESP_LOGI(DEBUG_TAG_DRIVER_LCD, "Lvgl Display Created");

    // Lvgl Tick Timer
    const esp_timer_create_args_t timer_args = {
        .callback = s_lvgl_tick_timer_cb,
        .name = "lv_tick"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_timer, DRIVER_LCD_LVGL_TICK_PERIOD_MS * 1000));

    // Create Lvgl Task
    xTaskCreate(
        s_task_lvgl,
        "t-lvgl",
        TASK_STACK_DEPTH_LVGL,
        NULL,
        TASK_PRIORITY_LVGL,
        &s_handle_task_lvgl
    );

    ESP_LOGI(DEBUG_TAG_DRIVER_LCD, "Lvgl Task Created");
    return true;
}

static void s_task_lvgl(void *arg)
{
    // LvgL Task

    util_dataqueue_item_t dq_i;

    ESP_LOGI(DEBUG_TAG_DRIVER_LCD, "Starting LVGL task");

    while(true){
        if(UTIL_DATAQUEUE_MessageCheck(&s_dataqueue))
        {
            if(UTIL_DATAQUEUE_MessageGet(&s_dataqueue, &dq_i, 0))
            {
                ESP_LOGI(DEBUG_TAG_DRIVER_LCD, "New In DataQueue. Type %u, Data %u", dq_i.data_type, dq_i.data);

                if(dq_i.data_type == DATA_TYPE_COMMAND)
                {
                    switch(dq_i.data)
                    {
                        case DRIVER_LCD_COMMAND_DEMO:
                            // Lvgl Demo
                            // For This To Work The Following Should Be Enabed Through Menuconfig
                            // 1. Component Config -> LVGL Configuration -> Demos -> Build Demos
                            // 1. Component Config -> LVGL Configuration -> Demos -> Benchmark Your System
                            // 1. Component Config -> LVGL Configuration -> Others -> Enable System Monitor Component
                            // 1. Component Config -> LVGL Configuration -> Others -> Show CPU Usage And Fps Count
                            #if defined CONFIG_LV_BUILD_DEMOS && defined CONFIG_LV_USE_SYSMON && defined CONFIG_LV_USE_PERF_MONITOR
                                ESP_LOGI(DEBUG_TAG_DRIVER_LCD, "Lvgl Demo", s_component_type);
                                lv_demo_benchmark();
                            #endif
                            break;
                        
                        case DRIVER_LCD_COMMAND_LOAD_UI:
                            #ifdef CONFIG_INCLUDE_UI
                            ui_init();
                            #endif
                            break;
                        
                        case DRIVER_LCD_COMMAND_SET_IP:
                            #ifdef CONFIG_INCLUDE_UI
                            lv_label_set_text(ui_ipaddress, dq_i.data_buff.value.ip);
                            if(dq_i.data_buff.value.ip[0] == '\0'){
                                lv_img_set_src(ui_imageconnection, &ui_img_images_button_red_png);
                            }else{
                                lv_image_set_src(ui_imageconnection, &ui_img_images_button_green_png);
                            }
                            #endif
                            break;

                        case DRIVER_LCD_COMMAND_SET_TIME:
                            #ifdef CONFIG_INCLUDE_UI
                            lv_label_set_text(ui_time, dq_i.data_buff.value.timedata.time_string);
                            lv_label_set_text(ui_date, dq_i.data_buff.value.timedata.date_string);
                            lv_label_set_text(ui_ampm, dq_i.data_buff.value.timedata.am_pm_string);
                            #endif
                            
                            // Start One Second Timer If Not Already Running
                            if(!esp_timer_is_active(s_timer_one_second)){
                                ESP_ERROR_CHECK(esp_timer_start_periodic(s_timer_one_second, 1000 * 1000));
                            }
                            break;

                        case DRIVER_LCD_COMMAND_SET_WEATHER:
                            #ifdef CONFIG_INCLUDE_UI
                            lv_label_set_text(ui_labelhumidity, dq_i.data_buff.value.weatherdata.humidity);
                            lv_label_set_text(uic_labelhtemperature, dq_i.data_buff.value.weatherdata.temp);
                            #endif
                            break;
                        
                        case DRIVER_LCD_COMMAND_SET_LOCATION:
                            #ifdef CONFIG_INCLUDE_UI
                            lv_label_set_text(ui_label1, dq_i.data_buff.value.location);
                            #endif
                            break;
                        
                        default:
                            break;
                    }
                }
            }
        }

        // Hacky Code For Second Toggling
        if(s_update_seconds){
            #ifdef CONFIG_INCLUDE_UI
            if(s_second_panel_visible){
                lv_obj_add_flag(ui_panel3, LV_OBJ_FLAG_HIDDEN);
            }else{
                lv_obj_clear_flag(ui_panel3, LV_OBJ_FLAG_HIDDEN);
            }
            s_second_panel_visible = !s_second_panel_visible;
            #endif
            s_update_seconds = false;
        }

        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(DRIVER_LCD_LVGL_TASK_PERIOD_MS));
    }
}

static void s_timer_one_second_cb(void *arg)
{
    // Send One Second Notification

    s_update_seconds = true;
}

static void s_lvgl_tick_timer_cb(void* arg)
{
    // Lvgl Tick Timer Cb

    // Tell Lvgl How Many Milliseconds Have Elapsed
    lv_tick_inc(DRIVER_LCD_LVGL_TICK_PERIOD_MS);
}

static bool s_lcd_rgb_panel_vsync_cb(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event_data, void *user_data)
{
    // Esp_lcd Panel Vsync Cb

    BaseType_t high_task_awoken = pdFALSE;

    // ESP_EARLY_LOGI(DEBUG_TAG_DRIVER_LCD, "vsync cb");
    xSemaphoreGiveFromISR(s_handle_semaphore_vsync, &high_task_awoken);

    return (high_task_awoken == pdTRUE);
}

static bool s_lcd_rgb_panel_color_trans_cb(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx)
{
    // Esp_lcd Panel Color Trans Done Cb
    // Tell Lvgl Ready To Swap Buffers

    // ESP_EARLY_LOGI(DEBUG_TAG_DRIVER_LCD, "color trans done cb");
    lv_display_flush_ready(s_lvgl_display);

    return false;
}

static void s_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    // Lvgl Flush Cb
    // Pass the Draw Buffer To The Driver

    // Wait For The VSync Event - With A Timeout
    // Forever Blocking Is Bad
    #if defined DRIVER_LCD_LVGL_USE_FULL_REFRESH
    if(xSemaphoreTake(s_handle_semaphore_vsync, pdMS_TO_TICKS(30)) != pdTRUE){
        ESP_LOGW(DEBUG_TAG_DRIVER_LCD, "VSYNC timeout");
    }
    #endif
    esp_lcd_panel_draw_bitmap(s_handle_rgb_panel,
        area->x1,
        area->y1,
        area->x2 + 1,
        area->y2 + 1,
        px_map
    );
    // lv_display_flush_ready(disp);
}