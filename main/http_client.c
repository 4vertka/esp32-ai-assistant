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
    static char* output_buffer;
    static int output_len;

    switch(evt->event_id)
    {
        case HTTP_EVENT_ERROR: {
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        }
        case HTTP_EVENT_ON_CONNECTED: {
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        }
        case HTTP_EVENT_HEADER_SENT: {
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        }
        case HTTP_EVENT_ON_HEADER: {
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        }
        case HTTP_EVENT_ON_HEADERS_COMPLETE: {
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADERS_COMPLETE");
            break;
        }
        case HTTP_EVENT_ON_DATA: {
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (output_len == 0 && evt->user_data) {
                memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
            }
            if (!esp_http_client_is_chunked_response(evt->client)) {
                int copy_len = 0;
                if (evt->user_data) {
                    copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                    if (copy_len) {
                        memcpy(evt->user_data + output_len, evt->data, copy_len);
                    }
                } else {
                    int content_len = esp_http_client_get_content_length(evt->client);
                    if (output_buffer == NULL) {
                        output_buffer = (char *) calloc(content_len + 1, sizeof(char));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    copy_len = MIN(evt->data_len, (content_len - output_len));
                    if (copy_len) {
                        memcpy(output_buffer + output_len, evt->data, copy_len);
                    }
                }
                output_len += copy_len;
            }

            break;
        }
        case HTTP_EVENT_ON_FINISH: {
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        }
        case HTTP_EVENT_DISCONNECTED: {
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        }
        case HTTP_EVENT_REDIRECT: {
            ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
            break;
        }
        default: break;
    }

    return ESP_OK;
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

