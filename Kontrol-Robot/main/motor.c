#include "motor.h"
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/ledc.h"

/* ===== PIN ===== */
#define IN1 15
#define IN2 16
#define IN3 18
#define IN4 19
#define ENA 17
#define ENB 14

/* ===== PWM ===== */
#define PWM_MAX        8191
#define PWM_START_MIN  4200
#define PWM_KICK       6500
#define KICK_TIME_MS   60

static bool motor_started = false;

/* ===================================================== */
void motor_init(void)
{
    /* Direction pins */
    gpio_reset_pin(IN1);
    gpio_reset_pin(IN2);
    gpio_reset_pin(IN3);
    gpio_reset_pin(IN4);

    gpio_set_direction(IN1, GPIO_MODE_OUTPUT);
    gpio_set_direction(IN2, GPIO_MODE_OUTPUT);
    gpio_set_direction(IN3, GPIO_MODE_OUTPUT);
    gpio_set_direction(IN4, GPIO_MODE_OUTPUT);

    /* PWM timer */
    ledc_timer_config_t timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = 3000,   // ⬅️ TURUNKAN
        .duty_resolution = LEDC_TIMER_13_BIT
    };
    ledc_timer_config(&timer);

    /* PWM channel kiri */
    ledc_channel_config_t ch0 = {
        .gpio_num   = ENA,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty       = 0
    };
    ledc_channel_config(&ch0);

    /* PWM channel kanan */
    ledc_channel_config_t ch1 = {
        .gpio_num   = ENB,
        .channel    = LEDC_CHANNEL_1,
        .timer_sel  = LEDC_TIMER_0,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty       = 0
    };
    ledc_channel_config(&ch1);
}

/* ===================================================== */
void motor_stop(void)
{
    motor_started = false;

    gpio_set_level(IN1, 0);
    gpio_set_level(IN2, 0);
    gpio_set_level(IN3, 0);
    gpio_set_level(IN4, 0);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

/* ===================================================== */
void motor_set(int left, int right)
{
    if (left  > 0 && left  < PWM_START_MIN) left  = PWM_START_MIN;
    if (right > 0 && right < PWM_START_MIN) right = PWM_START_MIN;

    if (left  > PWM_MAX) left  = PWM_MAX;
    if (right > PWM_MAX) right = PWM_MAX;

    /* MAJU */
    gpio_set_level(IN1, 1);
    gpio_set_level(IN2, 0);
    gpio_set_level(IN3, 1);
    gpio_set_level(IN4, 0);

    /* Kick-start SEKALI */
    if (!motor_started && (left || right))
    {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, PWM_KICK);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, PWM_KICK);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

        vTaskDelay(pdMS_TO_TICKS(KICK_TIME_MS));
        motor_started = true;
    }

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, left);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, right);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

