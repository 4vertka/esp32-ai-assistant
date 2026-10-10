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

static const char* TAG = "MAIN";

static const char* prompt = "Hello from esp32";

void app_main(void)
{


    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init();
    
    if (is_wifi_connected()) {
        static char recv_buffer[512] = {0};
        ai_init(prompt, recv_buffer, sizeof(recv_buffer));
        ESP_LOGI(TAG, "GEMINI: %s", recv_buffer);
    }else {
        ESP_LOGE(TAG, "Failed to connect to internet");
    }
}
