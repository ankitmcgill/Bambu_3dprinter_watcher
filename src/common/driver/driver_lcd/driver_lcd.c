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
#include "driver/ledc.h"
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

#ifdef DRIVER_LCD_HAS_PROJECT_UI
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
static uint8_t* s_rgb666_buf;
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
    // 240 x 320 pixel, RGB666 18-bit
    // ST7789T3 controller, SPI interface
    // Waveshare ESP32-S3 2.8inch Display Development Board, 240×320
    // https://www.waveshare.com/product/esp32-s3-touch-lcd-2.8.htm

    esp_err_t ret;

    // Backlight via LEDC PWM — start at 0% duty during init
    ledc_timer_config_t ledc_timer = {
        .speed_mode      = DRIVER_LCD_LEDC_MODE,
        .duty_resolution = DRIVER_LCD_LEDC_RESOLUTION,
        .timer_num       = DRIVER_LCD_LEDC_TIMER,
        .freq_hz         = DRIVER_LCD_LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .gpio_num   = BSP_LCD_GPIO_BL,
        .speed_mode = DRIVER_LCD_LEDC_MODE,
        .channel    = DRIVER_LCD_LEDC_CHANNEL,
        .timer_sel  = DRIVER_LCD_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    // SPI Bus
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = BSP_LCD_GPIO_MOSI,
        .miso_io_num     = -1,
        .sclk_io_num     = BSP_LCD_GPIO_SCK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = DRIVER_LCD_DISPLAY_HRES * DRIVER_LCD_DISPLAY_VRES * 3,
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

    // ST7789T3 Panel Device — 18-bit RGB666 mode
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BSP_LCD_GPIO_RST,
        .rgb_endian     = LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 18,
    };
    ret = esp_lcd_new_panel_st7789(s_io_handle, &panel_cfg, &s_panel_handle);
    ESP_GOTO_ON_ERROR(ret, err, DEBUG_TAG_DRIVER_LCD, "ST7789 panel init failed");

    // Reset, init, configure orientation
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_panel_handle, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel_handle, false, false));

    // Fill controller RAM with black before enabling display to prevent artifacts
    uint8_t *clear_buf = heap_caps_calloc(DRIVER_LCD_DISPLAY_HRES, 3, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(clear_buf);
    for(int y = 0; y < DRIVER_LCD_DISPLAY_VRES; y++) {
        esp_lcd_panel_draw_bitmap(s_panel_handle, 0, y, DRIVER_LCD_DISPLAY_HRES, y + 1, clear_buf);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    free(clear_buf);

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_handle, true));

    // Enable backlight at full brightness
    ESP_ERROR_CHECK(ledc_set_duty(DRIVER_LCD_LEDC_MODE, DRIVER_LCD_LEDC_CHANNEL, DRIVER_LCD_LEDC_DUTY_MAX));
    ESP_ERROR_CHECK(ledc_update_duty(DRIVER_LCD_LEDC_MODE, DRIVER_LCD_LEDC_CHANNEL));

    ESP_LOGI(DEBUG_TAG_DRIVER_LCD, "LCD Panel Setup OK");
    return true;

err:
    return false;
}

static bool s_lcd_flush_ready_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    // Called by SPI DMA ISR when a color transfer is complete
    // Signal LVGL that the flush buffer is free for the next frame

    if(s_lvgl_display) {
        lv_display_flush_ready(s_lvgl_display);
    }
    return false;
}

static void s_lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    // LVGL renders RGB565; ST7789T3 operates in 18-bit RGB666 mode.
    // Expand each 16-bit RGB565 pixel to 3 bytes (RRRRRR00, GGGGGG00, BBBBBB00).
    uint32_t pixel_count = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
    const uint16_t *src = (const uint16_t *)px_map;
    uint8_t *dst = s_rgb666_buf;
    for(uint32_t i = 0; i < pixel_count; i++){
        uint16_t px = src[i];
        uint8_t r5 = (px >> 11) & 0x1F;
        uint8_t g6 = (px >> 5)  & 0x3F;
        uint8_t b5 =  px        & 0x1F;
        *dst++ = ((r5 << 1) | (r5 >> 4)) << 2;
        *dst++ = g6 << 2;
        *dst++ = ((b5 << 1) | (b5 >> 4)) << 2;
    }

    // lv_display_flush_ready is called from s_lcd_flush_ready_cb once DMA completes
    esp_lcd_panel_draw_bitmap(s_panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, s_rgb666_buf);
}

static bool s_lvgl_setup(void)
{
    // Initialize Lvgl Port
    // SPI interface: partial refresh, two DMA-capable internal buffers of 100 lines each

    const size_t buf_size = 32 * DRIVER_LCD_DISPLAY_HRES * sizeof(lv_color16_t);

    lv_init();

    void* buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(buf1);
    void* buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(buf2);

    s_rgb666_buf = heap_caps_malloc(32 * DRIVER_LCD_DISPLAY_HRES * 3, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(s_rgb666_buf);

    s_lvgl_display = lv_display_create(DRIVER_LCD_DISPLAY_HRES, DRIVER_LCD_DISPLAY_VRES);
    assert(s_lvgl_display);

    lv_display_set_buffers(s_lvgl_display, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
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

    while(true)
    {
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
                            #if LV_USE_DEMO_WIDGETS
                            lv_demo_widgets();
                            #else
                            ESP_LOGW(DEBUG_TAG_DRIVER_LCD, "LV_USE_DEMO_WIDGETS not enabled in menuconfig");
                            #endif
                            break;

                        case DRIVER_LCD_COMMAND_LOAD_UI_SCREEN:
                            #ifdef DRIVER_LCD_HAS_PROJECT_UI
                            switch(dq_i.data_buff.value.uint8) {
                                case DRIVER_LCD_SCREEN_STARTUP:
                                    ui_init();
                                    break;
                                
                                case DRIVER_LCD_SCREEN_HOME:
                                    lv_scr_load(ui_Screen1);
                                    break;
                                
                                default:
                                    break;
                            }
                            ESP_LOGI(DEBUG_TAG_DRIVER_LCD, "DRIVER_LCD_COMMAND_LOAD_UI_SCREEN - %u", dq_i.data_buff.value.uint8);
                            #endif
                            break;

                        case DRIVER_LCD_COMMAND_REFRESH_UI:
                            break;

                        case DRIVER_LCD_COMMAND_SET_SCREEN2_MESSAGE:
                            #ifdef DRIVER_LCD_HAS_PROJECT_UI
                            lv_label_set_text(ui_Screen2LabelMessage, dq_i.data_buff.value.msg);
                            ESP_LOGI(DEBUG_TAG_DRIVER_LCD, "DRIVER_LCD_COMMAND_SET_SCREEN2_MESSAGE - %s", dq_i.data_buff.value.msg);
                            #endif
                            break;

                        case DRIVER_LCD_COMMAND_SET_SCREEN1_MESSAGE_STATUS:
                            #ifdef DRIVER_LCD_HAS_PROJECT_UI
                            lv_label_set_text(ui_Screen1LabelSTATUS, dq_i.data_buff.value.msg);
                            ESP_LOGI(DEBUG_TAG_DRIVER_LCD, "DRIVER_LCD_COMMAND_SET_SCREEN1_MESSAGE_STATUS - %s", dq_i.data_buff.value.msg);
                            #endif
                            break;

                        case DRIVER_LCD_COMMAND_SET_SCREEN1_PANEL_STATUS_COLOR:
                            #ifdef DRIVER_LCD_HAS_PROJECT_UI
                            {
                                lv_color_t color = (dq_i.data_buff.value.uint8 == DRIVER_LCD_STATUS_COLOR_GREEN)
                                    ? lv_color_hex(0x00FF00)
                                    : lv_color_hex(0xFF0000);
                                lv_obj_set_style_bg_color(ui_Sceen1PanelSTATUS, color, LV_PART_MAIN | LV_STATE_DEFAULT);
                                ESP_LOGI(DEBUG_TAG_DRIVER_LCD, "DRIVER_LCD_COMMAND_SET_SCREEN1_PANEL_STATUS_COLOR - %u", dq_i.data_buff.value.uint8);
                            }
                            #endif
                            break;

                        default:
                            break;
                    }
                }
            }
        }

        // Drive LVGL: process timers, animations, and trigger flush
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