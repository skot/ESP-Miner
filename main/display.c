#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "lvgl.h"
#include "lvgl__lvgl/src/themes/lv_theme_private.h"
#include "esp_lvgl_port.h"
#include "global_state.h"
#include "nvs_config.h"
#include "i2c_bitaxe.h"
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_lcd_panel_ssd1306.h"

#define SSD1306_I2C_ADDRESS    0x3C

#define LCD_H_RES              128
#define LCD_V_RES              32
#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8

static const char * TAG = "display";

static lv_theme_t theme;
static lv_style_t scr_style;

extern const lv_font_t lv_font_portfolio_6x8;

static void theme_apply(lv_theme_t *theme, lv_obj_t *obj) {
    if (lv_obj_get_parent(obj) == NULL) {
        lv_obj_add_style(obj, &scr_style, LV_PART_MAIN);
    }
}

// Dummy display driver callback
static void dummy_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    lv_display_flush_ready(disp);
}

static void init_styles(lv_disp_t * disp) {
    if (lvgl_port_lock(0)) {
        lv_style_init(&scr_style);
        lv_style_set_text_font(&scr_style, &lv_font_portfolio_6x8);
        lv_style_set_bg_opa(&scr_style, LV_OPA_COVER);

        lv_theme_set_apply_cb(&theme, theme_apply);
        lv_display_set_theme(disp, &theme);
        lvgl_port_unlock();
    }
}

esp_err_t display_init(void * pvParameters)
{
    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;
    GLOBAL_STATE->SYSTEM_MODULE.is_screen_active = false;

    // Initialize LVGL first
    ESP_LOGI(TAG, "Initialize LVGL");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL init failed");

    // Create a dummy display driver immediately after LVGL init
    static lv_display_t * dummy_disp;
    static lv_color_t dummy_buf[LCD_H_RES * 10];  // Smaller buffer for dummy display
    
    dummy_disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_buffers(dummy_disp, dummy_buf, NULL, sizeof(dummy_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(dummy_disp, dummy_flush_cb);
    init_styles(dummy_disp);

    // Try to initialize the actual display
    i2c_master_bus_handle_t i2c_master_bus_handle;
    if (i2c_bitaxe_get_master_bus_handle(&i2c_master_bus_handle) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get i2c master bus handle");
        return ESP_OK;  // Return OK since we have a dummy display
    }

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .scl_speed_hz = I2C_BUS_SPEED_HZ,
        .dev_addr = SSD1306_I2C_ADDRESS,
        .control_phase_bytes = 1,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .dc_bit_offset = 6                     
    };
    
    if (esp_lcd_new_panel_io_i2c(i2c_master_bus_handle, &io_config, &io_handle) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize panel IO, no display connected?");
        return ESP_OK;  // Return OK since we have a dummy display
    }

    ESP_LOGI(TAG, "Install SSD1306 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = -1,
    };

    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = LCD_V_RES,
    };
    panel_config.vendor_config = &ssd1306_config;

    if (esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to create panel driver");
        esp_lcd_panel_io_del(io_handle);
        return ESP_OK;
    }

    if (esp_lcd_panel_reset(panel_handle) != ESP_OK) {
        ESP_LOGW(TAG, "Panel reset failed");
        esp_lcd_panel_del(panel_handle);
        esp_lcd_panel_io_del(io_handle);
        return ESP_OK;
    }

    if (esp_lcd_panel_init(panel_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Panel init failed, no display connected?");
        esp_lcd_panel_del(panel_handle);
        esp_lcd_panel_io_del(io_handle);
        return ESP_OK;
    }

    uint8_t flip_screen = nvs_config_get_u16(NVS_CONFIG_FLIP_SCREEN, 1);
    uint8_t invert_screen = nvs_config_get_u16(NVS_CONFIG_INVERT_SCREEN, 0);

    if (esp_lcd_panel_invert_color(panel_handle, invert_screen) != ESP_OK) {
        ESP_LOGW(TAG, "Panel invert failed");
        esp_lcd_panel_del(panel_handle);
        esp_lcd_panel_io_del(io_handle);
        return ESP_OK;
    }
    
    // Delete the dummy display before creating the real one
    lv_disp_remove(dummy_disp);
    
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_H_RES * LCD_V_RES,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = true,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = false,
            .mirror_x = !flip_screen, // The screen is not flipped, this is for backwards compatibility
            .mirror_y = !flip_screen,
        },
        .flags = {
            .swap_bytes = false,
            .sw_rotate = false,
        }
    };

    lv_disp_t * disp = lvgl_port_add_disp(&disp_cfg);
    init_styles(disp);

    // Turn on the screen
    if (esp_lcd_panel_disp_on_off(panel_handle, true) != ESP_OK) {
        ESP_LOGW(TAG, "Panel display on failed");
        esp_lcd_panel_del(panel_handle);
        esp_lcd_panel_io_del(io_handle);
        return ESP_OK;
    }

    GLOBAL_STATE->SYSTEM_MODULE.is_screen_active = true;
    return ESP_OK;
}
