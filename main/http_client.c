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

static char *ai_create_prompt(const char *prompt)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON *contents = cJSON_AddArrayToObject(root, "contents");
    cJSON *content  = cJSON_CreateObject();
    cJSON_AddItemToArray(contents, content);
    cJSON *parts = cJSON_AddArrayToObject(content, "parts");
    cJSON *part  = cJSON_CreateObject();
    cJSON_AddItemToArray(parts, part);
    cJSON_AddStringToObject(part, "text", prompt);

    cJSON *gen = cJSON_AddObjectToObject(root, "generationConfig");
    cJSON_AddNumberToObject(gen, "maxOutputTokens", 100);
    cJSON *think = cJSON_AddObjectToObject(gen, "thinkingConfig");
    cJSON_AddStringToObject(think, "thinkingLevel", "minimal");

    char *json = cJSON_PrintUnformatted(root);  
    cJSON_Delete(root);                         
    return json;
}

static void ai_get_response(const char* json, char* out, size_t out_size) {
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        ESP_LOGE(TAG, "JSON parse failed");
        return;
    }

    cJSON *candidates = cJSON_GetObjectItem(root, "candidates");
    cJSON *cand0   = cJSON_GetArrayItem(candidates, 0);
    cJSON *content = cJSON_GetObjectItem(cand0, "content");
    cJSON *parts   = cJSON_GetObjectItem(content, "parts");
    cJSON *part0   = cJSON_GetArrayItem(parts, 0);
    cJSON *text    = cJSON_GetObjectItem(part0, "text");

    if (cJSON_IsString(text)) {
        snprintf(out, out_size, "%s", text->valuestring); 
    } else {
        ESP_LOGE(TAG, "No text in response");
    }

    cJSON_Delete(root);
}

void ai_ask(const char* prompt, char* out, size_t out_size) {
    if (out_size > 0) {
        out[0] = '\0';
    }

    char* json_prompt = ai_create_prompt(prompt);
    if (!json_prompt) {
        ESP_LOGE(TAG, "Failed to create prompt");
        return;
    }

    memset(response_buffer, 0, sizeof(response_buffer));

    esp_http_client_set_post_field(client, json_prompt, strlen(json_prompt));
    esp_err_t err = esp_http_client_perform(client);

    cJSON_free(json_prompt);

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP status: %d", status);

        if (status == 200) {
            ai_get_response(response_buffer, out, out_size);
        } else {
            ESP_LOGE(TAG, "ERROR: %s", response_buffer);
        }
    } else {
        ESP_LOGE(TAG, "perform failed: %s", esp_err_to_name(err));
    }
}

void ai_init(void) {
    esp_http_client_config_t config = {
        .host = "generativelanguage.googleapis.com",
        .path = "/v1beta/models/" GEMINI_MODEL ":generateContent",
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
    esp_http_client_set_header(client, "x-goog-api-key", GEMINI_API_KEY);
}


void ai_deinit(void) {
    if (client) {
        esp_http_client_cleanup(client);
        client = NULL;
    }
}
