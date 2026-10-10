#include "http_client.h"

#include <sys/param.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

#include "cJSON.h"

#include "secret.h"

#define MAX_HTTP_RECEIVE_BUFFER           512
#define MAX_HTTP_OUTPUT_BUFFER            8192

#define GEMINI_MODEL "gemini-3.5-flash-lite"

static const char* TAG = "HTTP_CLIENT";
static char response_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
static esp_http_client_handle_t client;

static esp_err_t http_event_handle(esp_http_client_event_t *evt) {
    static int output_len = 0;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            output_len = 0;                     
            if (evt->user_data) {
                ((char *)evt->user_data)[0] = '\0';
            }
            break;

        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HEADER %s: %s", evt->header_key, evt->header_value);
            break;

        case HTTP_EVENT_ON_DATA: {
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            char *buf = (char *)evt->user_data;
            if (buf && output_len + evt->data_len < MAX_HTTP_OUTPUT_BUFFER - 1) {
                memcpy(buf + output_len, evt->data, evt->data_len);
                output_len += evt->data_len;
                buf[output_len] = '\0';
            } else {
                ESP_LOGW(TAG, "Response buffer full, data dropped");
            }
            break;
        }

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;                                

        case HTTP_EVENT_DISCONNECTED: {
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error(
                (esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            break;
        }

        case HTTP_EVENT_REDIRECT:
            esp_http_client_set_redirection(evt->client);
            break;

        default:
            break;
    }
    return ESP_OK;
}

static char* ai_create_prompt(void) {

    /*
    // GEMINI JSON REQUEST FORMAT
    {
        "contents": {
            "role": "ROLE",
            "parts": {
                "text": "TEXT"
            }
        },
        "generation_config": {
            "responseMimeType": "RESPONSE_MIME_TYPE",
            "responseSchema": RESPONSE_SCHEMA,
        }
    }
    */

    cJSON *root = cJSON_CreateObject();

    cJSON *contents = cJSON_AddArrayToObject(root, "contents");
    cJSON *content  = cJSON_CreateObject();
    cJSON_AddItemToArray(contents, content);

    cJSON *parts = cJSON_AddArrayToObject(content, "parts");
    cJSON *part  = cJSON_CreateObject();
    cJSON_AddItemToArray(parts, part);
    cJSON_AddStringToObject(part, "text", "Hello from ESP");

    cJSON *gen = cJSON_AddObjectToObject(root, "generationConfig");
    cJSON_AddStringToObject(gen, "responseMimeType", "application/json");

    char *str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return str;   
}

static void ai_get_response(const char* json, char* buffer, size_t buffer_size) {
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON parse failed");
        return;
    }

    cJSON *candidates = cJSON_GetObjectItem(root, "candidates");
    cJSON *cand0      = cJSON_GetArrayItem(candidates, 0);
    cJSON *content    = cJSON_GetObjectItem(cand0, "content");
    cJSON *parts      = cJSON_GetObjectItem(content, "parts");
    cJSON *part0      = cJSON_GetArrayItem(parts, 0);
    cJSON *text       = cJSON_GetObjectItem(part0, "text");

    if (cJSON_IsString(text) && text->valuestring != NULL) {
        snprintf(buffer, buffer_size, "%s", text->valuestring);
    } else {
        ESP_LOGE(TAG, "No text in response");
    }

    cJSON_Delete(root);
}

void ai_init(const char* prompt, char* out_buffer, size_t out_buffer_size) {


    esp_http_client_config_t config = {
        .host = "generativelanguage.googleapis.com",
        .path = "/v1beta/models/" GEMINI_MODEL ":generateContent",
        .query = "key=" GEMINI_API_KEY, 
        .event_handler = http_event_handle,
        .user_data = response_buffer,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30000
    };

    ESP_LOGI(TAG, "HTTPS request with url =>");
    client = esp_http_client_init(&config);
    
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    char* post_data = ai_create_prompt();
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_err_t err = esp_http_client_perform(client);
    cJSON_free(post_data);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        if (esp_http_client_get_status_code(client) == 200) {
            ai_get_response(response_buffer, out_buffer, out_buffer_size);        
        }
        else {
            ESP_LOGE(TAG, "ERROR: %s", response_buffer);
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

