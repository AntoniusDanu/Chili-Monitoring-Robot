#ifndef WIFI_HTTP_H
#define WIFI_HTTP_H

#include "esp_err.h"
#include "freertos/queue.h"

/* ===== DHT DATA ===== */
typedef struct {
    float temperature;
    float humidity;
} dht_data_t;

/* ===== QUEUE ===== */
extern QueueHandle_t pot_queue;

/* ===== TASK ===== */
void wifi_http_task(void *pv);

#endif

