#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

#define IN1 25
#define IN2 26
#define IN3 27
#define IN4 14

#define ENA 32
#define ENB 12

static const char *TAG = "L298N";

#define PWM_FREQ_HZ     1000
#define PWM_DUTY_LOW    4096    // duty rendah (0–8191 untuk 13-bit)
#define PWM_RESOLUTION  LEDC_TIMER_13_BIT

/*
void motor_forward()
{
    ESP_LOGI(TAG, "Arah MAJU");

    gpio_set_level(IN1, 1);
    gpio_set_level(IN2, 0);

    gpio_set_level(IN3, 1);
    gpio_set_level(IN4, 0);
}
*/
void motor_forward()
{
    ESP_LOGI(TAG, "Arah MAJU");

    gpio_set_level(IN1, 1);
    gpio_set_level(IN2, 1);

    gpio_set_level(IN3, 0);
    gpio_set_level(IN4, 0);
}

void motor_backward()
{
    ESP_LOGI(TAG, "Arah MUNDUR");

    gpio_set_level(IN1, 0);
    gpio_set_level(IN2, 0);

    gpio_set_level(IN3, 0);
    gpio_set_level(IN4, 1);
}


void app_main(void)
{
    // Set pin arah motor
    gpio_set_direction(IN1, GPIO_MODE_OUTPUT);
    gpio_set_direction(IN2, GPIO_MODE_OUTPUT);
    gpio_set_direction(IN3, GPIO_MODE_OUTPUT);
    gpio_set_direction(IN4, GPIO_MODE_OUTPUT);

    // Konfigurasi timer PWM LEDC
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = PWM_RESOLUTION,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = PWM_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // Channel PWM ENA
    ledc_channel_config_t channel_ena = {
        .gpio_num   = ENA,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&channel_ena);

    // Channel PWM ENB
    ledc_channel_config_t channel_enb = {
        .gpio_num   = ENB,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_1,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&channel_enb);

    ESP_LOGI(TAG, "Mulai kontrol motor…");

    while (1)
    {
        // ----------------- MAJU 5 DETIK -----------------
        motor_forward();

        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, PWM_DUTY_LOW);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, PWM_DUTY_LOW);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

        vTaskDelay(pdMS_TO_TICKS(5000));

        // ----------------- MUNDUR 5 DETIK -----------------
        motor_backward();

        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, PWM_DUTY_LOW);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, PWM_DUTY_LOW);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

