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
#include "display_lcd.h"

static const char* TAG = "MAIN";

static const char* prompt = "Hello from esp32";

static void ai_task(void) {//void *args) {
    display_lcd_init();

    static char recv_buffer[32];
    ai_init();

    //while(1) {
        if (is_wifi_connected()) {
            ai_ask("hello: answer only in 31 bytes(letters)", recv_buffer, sizeof(recv_buffer));
            ESP_LOGI(TAG, "GEMINI: %s", recv_buffer);
          
            display_lcd_write(&recv_buffer);
            //vTaskDelay(pdMS_TO_TICKS(60000));
        }
    //}
}

static void display_task(void *args) {
    display_init();
    vTaskDelay(NULL);
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
