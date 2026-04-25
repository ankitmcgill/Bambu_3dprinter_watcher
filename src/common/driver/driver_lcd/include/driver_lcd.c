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
#include "driver/spi_master.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
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

// Private Defines

// Extern Variables

// Local Variables
static TaskHandle_t s_handle_task_lvgl;
static rtos_component_type_t s_component_type;
static util_dataqueue_t s_dataqueue;
static esp_timer_handle_t s_timer;
static esp_timer_handle_t s_timer_one_second;
static esp_lcd_panel_io_handle_t s_io_handle;
static esp_lcd_panel_handle_t s_panel_handle;
static lv_display_t* s_lvgl_display;
static volatile bool s_update_seconds;

// Local Functions
static bool s_lcd_panel_setup(void);
static bool s_lvgl_setup(void);
static bool s_lcd_flush_ready_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
static void s_lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map);
static void s_task_lvgl(void *arg);
static void s_timer_one_second_cb(void *arg);
static void s_lvgl_tick_timer_cb(void* arg);

// External Functions
bool DRIVER_LCD_Init(void)
{
    // Initialize Driver Lcd

    s_component_type = COMPONENT_TYPE_TASK;

    ESP_LOGI(DEBUG_TAG_DRIVER_LCD, "Type %u. Init", s_component_type);

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
    if(!s_lcd_panel_setup()) goto err;
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

static bool s_lcd_panel_setup(void)
{
    // Initialize LCD Panel
    // 170 x 320 pixel, RGB565 16-bit
    // ST7789 controller, SPI interface
    // ST7789 chip RAM is 240 columns wide; a 170-wide panel sits at column offset 35

    esp_err_t ret;

    // Backlight off during init
    gpio_config_t bl_cfg = {
        .pin_bit_mask = BIT64(BSP_LCD_GPIO_BL),
        .mode         = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&bl_cfg));
    gpio_set_level(BSP_LCD_GPIO_BL, 0);

    // SPI Bus
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = BSP_LCD_GPIO_MOSI,
        .miso_io_num     = -1,
        .sclk_io_num     = BSP_LCD_GPIO_SCK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = DRIVER_LCD_DISPLAY_HRES * DRIVER_LCD_DISPLAY_VRES * sizeof(uint16_t),
    };
    ret = spi_bus_initialize(DRIVER_LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    ESP_GOTO_ON_ERROR(ret, err, DEBUG_TAG_DRIVER_LCD, "SPI bus init failed");

    // SPI Panel IO — on_color_trans_done fires after each DMA pixel transfer,
    // letting LVGL know the flush buffer is free (async flush)
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num         = BSP_LCD_GPIO_DC,
        .cs_gpio_num         = BSP_LCD_GPIO_CS,
        .pclk_hz             = DRIVER_LCD_SPI_CLK_HZ,
        .lcd_cmd_bits        = 8,
        .lcd_param_bits      = 8,
        .spi_mode            = 0,
        .trans_queue_depth   = 10,
        .on_color_trans_done = s_lcd_flush_ready_cb,
        .user_ctx            = NULL,
    };
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)DRIVER_LCD_SPI_HOST, &io_cfg, &s_io_handle);
    ESP_GOTO_ON_ERROR(ret, err, DEBUG_TAG_DRIVER_LCD, "Panel IO init failed");

    // ST7789 Panel Device
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BSP_LCD_GPIO_RST,
        .rgb_endian     = LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 16,
    };
    ret = esp_lcd_new_panel_st7789(s_io_handle, &panel_cfg, &s_panel_handle);
    ESP_GOTO_ON_ERROR(ret, err, DEBUG_TAG_DRIVER_LCD, "ST7789 panel init failed");

    // Reset, init, configure orientation
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_panel_handle, 35, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_handle, true));

    // Enable backlight
    gpio_set_level(BSP_LCD_GPIO_BL, 1);

    ESP_LOGI(DEBUG_TAG_DRIVER_LCD, "LCD Panel Setup OK");
    return true;

err:
    return false;
}

static bool s_lcd_flush_ready_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    // Called by SPI DMA ISR when a color transfer is complete
    // Signal LVGL that the flush buffer is free for the next frame

    lv_display_flush_ready(s_lvgl_display);
    return false;
}

static void s_lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    // Copy LVGL render buffer to the display over SPI DMA (non-blocking)
    // lv_display_flush_ready is called from s_lcd_flush_ready_cb once DMA completes

    esp_lcd_panel_draw_bitmap(s_panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
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
                            break;
                        
                        case DRIVER_LCD_COMMAND_LOAD_UI:
                            break;
                        
                        case DRIVER_LCD_COMMAND_REFRESH_UI:
                            break;
                        
                        default:
                            break;
                    }
                }
            }
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