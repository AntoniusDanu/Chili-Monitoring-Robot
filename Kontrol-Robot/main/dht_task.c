#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "dht.h"
#include "wifi_http.h"

#define TAG "DHT_TASK"

#define DHT_PIN     GPIO_NUM_11
#define DHT_TYPE    DHT_TYPE_AM2301   // DHT22

extern QueueHandle_t dht_queue;

void dht_task(void *pv)
{
    dht_data_t data;
    esp_err_t res;

    // ------------------------------------------------
    // 1️⃣ Cek awal: apakah DHT benar-benar ada
    // ------------------------------------------------
    res = dht_read_float_data(
        DHT_TYPE,
        DHT_PIN,
        &data.humidity,
        &data.temperature
    );

    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "DHT not detected (%s). Task suspended.",
                 esp_err_to_name(res));

        vTaskSuspend(NULL);   // ⬅️ PENTING: stop task permanen
    }

    ESP_LOGI(TAG, "DHT detected, task running");

    // ------------------------------------------------
    // 2️⃣ Loop normal
    // ------------------------------------------------
    while (1)
    {
        res = dht_read_float_data(
            DHT_TYPE,
            DHT_PIN,
            &data.humidity,
            &data.temperature
        );

        if (res == ESP_OK)
        {
            ESP_LOGI(TAG, "Temp=%.2f°C Hum=%.2f%%",
                     data.temperature, data.humidity);

            xQueueSend(dht_queue, &data, 0);
        }
        else
        {
            static uint8_t fail_count = 0;

            if (fail_count < 3)
            {
                ESP_LOGW(TAG, "DHT read failed: %s",
                         esp_err_to_name(res));
                fail_count++;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

