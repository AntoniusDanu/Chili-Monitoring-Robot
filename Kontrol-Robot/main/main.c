#include <stdio.h>
#include <stddef.h>

/* FreeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

/* ESP-IDF */
#include "esp_log.h"
#include "nvs_flash.h"

/* Project headers */
#include "motor.h"
#include "linefollow.h"
#include "hcsr.h"
#include "dht_task.h"
#include "wifi_http.h"
#include "robot_state.h"

/* ===== GLOBAL ===== */
QueueHandle_t dht_queue;
SemaphoreHandle_t log_mutex;

/* =========================================================
 *  TASK MONITOR
 * ========================================================= */
static void monitor_task(void *pv)
{
    char stats[512];

    while (1)
    {
        vTaskGetRunTimeStats(stats);
        ESP_LOGI("RTOS_STATS", "\nTask Runtime Stats:\n%s", stats);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* =========================================================
 *  APP MAIN
 * ========================================================= */
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    robot_state = ROBOT_RUN;

    /* ===== Mutex log ===== */
    log_mutex = xSemaphoreCreateMutex();

    /* ===== Init hardware ===== */
    motor_init();

    /* ===== Queue DHT ===== */
    dht_queue = xQueueCreate(4, sizeof(dht_data_t));
    if (!dht_queue)
    {
        ESP_LOGE("MAIN", "Failed to create DHT queue");
        return;
    }

    /* ===== TASKS ===== */
    xTaskCreatePinnedToCore(linefollow_task, "line", 4096, NULL, 6, NULL, 1);
    xTaskCreatePinnedToCore(hcsr_task,       "hcsr", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(wifi_http_task,  "wifi", 6144, NULL, 4, NULL, 0);
    xTaskCreatePinnedToCore(dht_task,        "dht",  4096, NULL, 3, NULL, 1);

    /* ===== MONITOR ===== */
    xTaskCreate(monitor_task, "monitor", 4096, NULL, 1, NULL);
}

