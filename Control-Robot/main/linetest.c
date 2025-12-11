#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define SENSOR_COUNT 8

// pin QTR-8RC
static const gpio_num_t qtr_pins[SENSOR_COUNT] = {
    GPIO_NUM_18,
    GPIO_NUM_19,
    GPIO_NUM_21,
    GPIO_NUM_22,
    GPIO_NUM_23,
    GPIO_NUM_2,
    GPIO_NUM_13,
    GPIO_NUM_16
};

// buffer hasil pembacaan sensor
uint32_t sensor_readings[SENSOR_COUNT];

// waktu maksimal discharge (us)
#define TIMEOUT_US 3000
#define EMITTER_PIN GPIO_NUM_4

void qtr_init()
{
    // emitter / LED IR
    gpio_reset_pin(EMITTER_PIN);
    gpio_set_direction(EMITTER_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(EMITTER_PIN, 1);  // nyalakan IR LED

    // set semua sensor sebagai output LOW
    for (int i = 0; i < SENSOR_COUNT; i++)
    {
        gpio_reset_pin(qtr_pins[i]);
        gpio_set_direction(qtr_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(qtr_pins[i], 0);
    }
}

void qtr_read_sensors()
{
    // 1. Charge kapasitor internal sensor → HIGH 10us
    for (int i = 0; i < SENSOR_COUNT; i++)
    {
        gpio_set_direction(qtr_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(qtr_pins[i], 1);
    }

    esp_rom_delay_us(10);  // ganti ets_delay_us()

    // 2. Set sensor sebagai input (mulai discharge)
    for (int i = 0; i < SENSOR_COUNT; i++)
    {
        gpio_set_direction(qtr_pins[i], GPIO_MODE_INPUT);
    }

    // 3. Hitung waktu discharge (HIGH → LOW)
    uint64_t start_time = esp_timer_get_time();

    int done = 0;
    while (!done)
    {
        done = 1;
        uint64_t now = esp_timer_get_time();
        uint32_t elapsed = now - start_time;

        for (int i = 0; i < SENSOR_COUNT; i++)
        {
            if (sensor_readings[i] == 0)
            {
                if (gpio_get_level(qtr_pins[i]) == 0)
                {
                    sensor_readings[i] = elapsed;
                }
                else if (elapsed >= TIMEOUT_US)
                {
                    sensor_readings[i] = TIMEOUT_US;
                }
                else
                {
                    done = 0;
                }
            }
        }

        if (elapsed >= TIMEOUT_US)
            break;
    }
}

void app_main(void)
{
    qtr_init();
    vTaskDelay(pdMS_TO_TICKS(1000));

    while (1)
    {
        // reset buffer
        for (int i = 0; i < SENSOR_COUNT; i++)
            sensor_readings[i] = 0;

        qtr_read_sensors();

        // tampilkan nilai sensor
        printf("QTR: ");
        for (int i = 0; i < SENSOR_COUNT; i++)
        {
            printf("%4lu ", sensor_readings[i]);
        }
        printf("\n");

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

