#include "hcsr.h"
#include "robot_state.h"

#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

/* ===== PIN CONFIG ===== */
#define TRIG_PIN GPIO_NUM_12
#define ECHO_PIN GPIO_NUM_13

/* ===== PARAM ===== */
#define TIMEOUT_US 30000   // 30 ms
#define STOP_CM    5

static const char *TAG = "HCSR";

/* ===== INIT ===== */
void hcsr_init(void)
{
    gpio_config_t io = {};

    // TRIG OUTPUT
    io.pin_bit_mask = 1ULL << TRIG_PIN;
    io.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io);

    // ECHO INPUT
    io.pin_bit_mask = 1ULL << ECHO_PIN;
    io.mode = GPIO_MODE_INPUT;
    io.pull_down_en = 0;
    gpio_config(&io);

    gpio_set_level(TRIG_PIN, 0);
}

int hcsr_read_cm(void)
{
    int64_t t_start, t_end;

    // TRIG 10 us
    gpio_set_level(TRIG_PIN, 0);
    esp_rom_delay_us(5);
    gpio_set_level(TRIG_PIN, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG_PIN, 0);

    int64_t t0 = esp_timer_get_time();

    // wait echo HIGH
    while (gpio_get_level(ECHO_PIN) == 0)
    {
        if (esp_timer_get_time() - t0 > 30000)
            return -1;
    }

    t_start = esp_timer_get_time();

    // wait echo LOW
    while (gpio_get_level(ECHO_PIN) == 1)
    {
        if (esp_timer_get_time() - t_start > 30000)
            return -1;
    }

    t_end = esp_timer_get_time();

    int d = (int)((t_end - t_start) / 58);

    // filter jarak tidak valid
    if (d < 2 || d > 400)
        return -2;

    return d;
}


/* ===== SAFE LOG ===== */
static void hcsr_log(int d)
{
#if HCSR_TEST_LOG
    if (log_mutex && xSemaphoreTake(log_mutex, pdMS_TO_TICKS(20)))
    {
        if (d < 0)
            ESP_LOGW(TAG, "Timeout / no echo");
        else
            ESP_LOGI(TAG, "Distance: %d cm", d);

        xSemaphoreGive(log_mutex);
    }
#endif
}

/* ===== TASK ===== */
void hcsr_task(void *pv)
{
    hcsr_init();

    while (1)
    {
        int d = hcsr_read_cm();

        hcsr_log(d);

        if (d > 0 && d < STOP_CM)
            robot_state = ROBOT_STOP;
        else
            robot_state = ROBOT_RUN;

        vTaskDelay(pdMS_TO_TICKS(200));   // aman untuk RTOS & DHT
    }
}

