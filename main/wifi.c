#include "wifi.h"

#include "secret.h"

#include <complex.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include "esp_system.h"
#include <string.h>

#include "lwip/err.h"
#include "lwip/sys.h"

#include "http_client.h"

static const char *WIFI_TAG = "WIFI";
static EventGroupHandle_t wifi_event_group;

#define WIFI_CONNECTED_BIT          BIT0
#define WIFI_FAIL_BIT               BIT1

static int max_connect_retry = 5;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (max_connect_retry < 5) {
      esp_wifi_connect();
      max_connect_retry++;
      ESP_LOGI(WIFI_TAG, "Connecting failed, retrying");
    } else {
      xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGE(WIFI_TAG, "Connection failed");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(WIFI_TAG, "ip: " IPSTR, IP2STR(&event->ip_info.ip));
    max_connect_retry = 0;
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

uint8_t is_wifi_connected(void) {
    if (xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT) {
        return 1;
    }
    return 0;
}

void wifi_init(void) {
  wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

  wifi_config_t wifi_config = {
    .sta =
      {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWD,
        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
       },
  };

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

  ESP_ERROR_CHECK(esp_wifi_start());
  esp_wifi_set_ps(WIFI_PS_NONE);

  ESP_LOGI(WIFI_TAG, "WIFI INIT FINISHED");

  EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE,
  portMAX_DELAY);

  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(WIFI_TAG, "Connected to wifi SSID:%s, password:%s", WIFI_SSID, WIFI_PASSWD);
  }else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGI(WIFI_TAG, "Failed to connect to wifi SSID:%s, password:%s", WIFI_SSID, WIFI_PASSWD);
  }else {
      ESP_LOGE(WIFI_TAG, "UNEXPECTED EVENT");
  }

}
