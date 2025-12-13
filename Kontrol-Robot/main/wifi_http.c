#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_http_client.h"

#include "wifi_http.h"
#include "robot_state.h"

#define TAG "WIFI_HTTP"

#define WIFI_SSID "vivo Y33S"
#define WIFI_PASS "arduinouno"

/* ===== ENDPOINT ===== */
#define POST_DHT_URL   "http://leafiot.ksmiotupnvj.com:8000/sensor/dht"
#define POST_POT_URL   "http://leafiot.ksmiotupnvj.com:8000/pot"
#define CHECK_UPLOAD   "http://leafiot.ksmiotupnvj.com:8000/chili/upload"

extern QueueHandle_t dht_queue;
QueueHandle_t pot_queue;

static int pot_index = 0;

/* ================= WIFI HANDLER ================= */
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_id == WIFI_EVENT_STA_START)
        esp_wifi_connect();

    else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        esp_wifi_connect();

    else if (event_base == IP_EVENT &&
             event_id == IP_EVENT_STA_GOT_IP)
        ESP_LOGI(TAG, "WiFi connected");
}

/* ================= HTTP POST DHT ================= */
static void http_post_dht(dht_data_t *data)
{
    char json[128];

    snprintf(json, sizeof(json),
             "{\"device\":\"esp32cam-01\",\"temperature\":%.2f,\"humidity\":%.2f}",
             data->temperature, data->humidity);

    esp_http_client_config_t cfg = {
        .url = POST_DHT_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json, strlen(json));

    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

/* ================= HTTP POST POT ================= */
static bool http_post_pot(int pot)
{
    char json[32];
    snprintf(json, sizeof(json), "{\"pot\":%d}", pot);

    esp_http_client_config_t cfg = {
        .url = POST_POT_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json, strlen(json));

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    return (err == ESP_OK && status == 200);
}

/* ================= WAIT UPLOAD ================= */
static bool wait_upload_done(void)
{
    esp_http_client_config_t cfg = {
        .url = CHECK_UPLOAD,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    return (err == ESP_OK && status == 200);
}

/* ================= TASK ================= */
void wifi_http_task(void *pv)
{
    dht_data_t recv;
    int pot_event;

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                               &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                               &wifi_event_handler, NULL);

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        }
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();

    pot_queue = xQueueCreate(2, sizeof(int));

    while (1)
    {
        /* === DHT === */
        if (xQueueReceive(dht_queue, &recv, 0))
            http_post_dht(&recv);

        /* === POT EVENT === */
        if (xQueueReceive(pot_queue, &pot_event, 0))
        {
            ESP_LOGI(TAG, "Pot %d detected", pot_index);

            if (http_post_pot(pot_index))
            {
                ESP_LOGI(TAG, "Waiting upload...");

                if (wait_upload_done())
                {
                    ESP_LOGI(TAG, "Upload OK, continue");
                    pot_index++;
                    robot_state = ROBOT_RUN;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

