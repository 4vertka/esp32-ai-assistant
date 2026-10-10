#include "http_client.h"

#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "mbedtls/ssl.h"
#include "esp_crt_bundle.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "esp_http_client.h"

#include "secret.h"

#define MAX_HTTP_RECEIVE_BUFFER         512
#define MAX_HTTP_OUTPUT_BUFFER            8192

#define GEMINI_MODEL "gemini-3.5-flash-lite"

static const char* TAG = "HTTP_CLIENT";
static const char response_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};

static esp_err_t http_event_handle(esp_http_client_event_t *evt) {

}

void ai_init(void) {
   
    esp_http_client_config_t config = {
        .url = "generativelanguage.googleapis.com",
        .path = "/v1beta/models/" GEMINI_MODEL ":generateContent",
        .query = GEMINI_API_KEY, 
        .event_handler = http_event_handle,
        .user_data = response_buffer,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30000
    };

    ESP_LOGI(TAG, "HTTPS request with url =>");
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    //esp_http_client_cleanup(client);
}

