#include "display.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ssd1306.h"

#include "esp_log.h"

#include "driver/i2c_master.h"

#include "bitmaps.h"

static const i2c_port_num_t i2c_port = 0;
static const gpio_num_t i2c_sda_pin = 21;
static const gpio_num_t i2c_scl_pin = 22;
static const uint8_t i2c_glitch_ignore_count = 7;

static const char* TAG = "DEVICE";

void display_init(void) {
    esp_err_t ret;
    i2c_master_bus_handle_t i2c_bus_handle;

    i2c_master_bus_config_t bus_config = {
        .i2c_port = i2c_port,
        .sda_io_num = i2c_sda_pin,
        .scl_io_num = i2c_scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = i2c_glitch_ignore_count,
        .flags.enable_internal_pullup = true,
    };

    ret = i2c_new_master_bus(&bus_config, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2C");
        return;
    }

    ssd1306_config_t dev_cfg = I2C_SSD1306_128x64_CONFIG_DEFAULT;
    ssd1306_handle_t dev_hdl = NULL;

    ssd1306_init(i2c_bus_handle, &dev_cfg, &dev_hdl);
    if (dev_hdl == NULL) {
        ESP_LOGE(TAG, "ssd1306 handle init failed");
        return;
    }

    ssd1306_set_contrast(dev_hdl, 0xff);
    ssd1306_clear_display(dev_hdl, false);

    ESP_LOGI(TAG, "Panel is 128x64");
    ESP_LOGI(TAG, "Display bitmap icons");

    for (;;) {
        for (int i = 0; i < ANIM_FRAMES; i++) {
            ssd1306_display_bitmap(dev_hdl, 0, 0, anim_frames[i], 128, 64, false);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

