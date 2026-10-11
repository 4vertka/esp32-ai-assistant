#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include <stdio.h>

#include "nvs_flash.h"
#include "esp_log.h" 
#include "esp_wifi.h"

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "wifi.h"
#include "http_client.h"

#include "display.h"

#include <hd44780.h>

static const char* TAG = "MAIN";

static const char* prompt = "Hello from esp32";

static void ai_task(void) {//void *args) {
    // lcd display part
    hd44780_t lcd = {
          .write_cb = NULL,
                  .font = HD44780_FONT_5X8,
          .lines = 2,
          .pins = {
              .rs = 19,
              .e  = 23,
              .d4 = 18,
              .d5 = 17,
            .d6 = 16,
            .d7 = 15,
            .bl = HD44780_NOT_USED
        },
    };
    
    ESP_ERROR_CHECK(hd44780_init(&lcd));

    // ai part
    static char recv_buffer[32];
    ai_init();

    //while(1) {
        if (is_wifi_connected()) {
            ai_ask("hello: answer only in 31 bytes(letters)", recv_buffer, sizeof(recv_buffer));
            ESP_LOGI(TAG, "GEMINI: %s", recv_buffer);
            
            char *top_line_buffer = &recv_buffer[0];
            char *bottom_line_buffer = &recv_buffer[16];

            hd44780_puts(&lcd, top_line_buffer);
            hd44780_gotoxy(&lcd, 0, 1);         
            hd44780_puts(&lcd, bottom_line_buffer);
            vTaskDelay(pdMS_TO_TICKS(60000));
        }
    //}
}

static void display_task(void *args) {
    display_init();
    vTaskDelay(NULL);
}

static void display_lcd_task() {
   hd44780_t lcd = {
        .write_cb = NULL,
                .font = HD44780_FONT_5X8,
        .lines = 2,
        .pins = {
            .rs = 19,
            .e  = 23,
            .d4 = 18,
            .d5 = 17,
            .d6 = 16,
            .d7 = 15,
            .bl = HD44780_NOT_USED
        },
    };

    ESP_ERROR_CHECK(hd44780_init(&lcd));
    
    hd44780_puts(&lcd, "Hello1");
    hd44780_gotoxy(&lcd, 0, 1);        
    hd44780_puts(&lcd, "Hello2");

}

void app_main(void)
{


    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    //display_lcd_task();

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init();
    
    ai_task();

    xTaskCreate(display_task, "display", 4096, NULL, 5, NULL);
    //xTaskCreate(ai_task, "ai", 8192, NULL, 5, NULL);
    //xTaskCreate(display_lcd_task, "display_lcd", 4096, NULL, 5, NULL);
}
