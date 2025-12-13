#include "qtr.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

#define TIMEOUT_US 3000

// ================= PIN QTR =================
static const gpio_num_t qtr_pins[QTR_SENSOR_COUNT] = {
    GPIO_NUM_38,
    GPIO_NUM_39,
    GPIO_NUM_1,
    GPIO_NUM_2,
    GPIO_NUM_3,
    GPIO_NUM_4,
    GPIO_NUM_5,
    GPIO_NUM_6
};

// buffer internal
static uint32_t sensor_values[QTR_SENSOR_COUNT];

// ==========================================
void qtr_init(void)
{
    for (int i = 0; i < QTR_SENSOR_COUNT; i++)
    {
        gpio_reset_pin(qtr_pins[i]);
        gpio_set_direction(qtr_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(qtr_pins[i], 0);
    }
}

// ==========================================
void qtr_read_raw(uint32_t *values)
{
    // reset buffer
    for (int i = 0; i < QTR_SENSOR_COUNT; i++)
        sensor_values[i] = 0;

    // 1. charge capacitor
    for (int i = 0; i < QTR_SENSOR_COUNT; i++)
    {
        gpio_set_direction(qtr_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(qtr_pins[i], 1);
    }

    esp_rom_delay_us(10);

    // 2. set input (discharge)
    for (int i = 0; i < QTR_SENSOR_COUNT; i++)
        gpio_set_direction(qtr_pins[i], GPIO_MODE_INPUT);

    uint64_t start = esp_timer_get_time();

    while (1)
    {
        uint64_t now = esp_timer_get_time();
        uint32_t elapsed = now - start;
        int done = 1;

        for (int i = 0; i < QTR_SENSOR_COUNT; i++)
        {
            if (sensor_values[i] == 0)
            {
                if (gpio_get_level(qtr_pins[i]) == 0)
                    sensor_values[i] = elapsed;
                else if (elapsed >= TIMEOUT_US)
                    sensor_values[i] = TIMEOUT_US;
                else
                    done = 0;
            }
        }

        if (done || elapsed >= TIMEOUT_US)
            break;
    }

    for (int i = 0; i < QTR_SENSOR_COUNT; i++)
        values[i] = sensor_values[i];
}

// ==========================================
// POSISI GARIS (0 – 7000)
// hitam = nilai besar
// ==========================================
int qtr_read_position(void)
{
    uint32_t values[QTR_SENSOR_COUNT];
    qtr_read_raw(values);

    uint32_t weighted_sum = 0;
    uint32_t sum = 0;

    for (int i = 0; i < QTR_SENSOR_COUNT; i++)
    {
        uint32_t v = values[i];
        weighted_sum += v * (i * 1000);
        sum += v;
    }

    if (sum == 0)
        return -1;   // garis hilang

    return weighted_sum / sum;
}

