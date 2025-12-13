#include "linefollow.h"
#include "motor.h"
#include "robot_state.h"
#include "qtr.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define KP 0.018f
#define KD 0.0001f

#define BASE_SPEED 3500
#define MAX_SPEED  6000
#define CENTER     3500

static int last_error = 0;

static int clamp(int val, int min, int max)
{
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

void linefollow_task(void *pv)
{
    qtr_init();   // PENTING

    while (1)
    {
        switch (robot_state)
        {
            case ROBOT_RUN:
            {
                int pos = qtr_read_position();

                // ===== garis hilang =====
                if (pos < 0)
                {
                    motor_stop();
                    break;
                }

                int error = pos - CENTER;
                int corr  = (int)(KP * error + KD * (error - last_error));
                last_error = error;

                int left  = BASE_SPEED - corr;
                int right = BASE_SPEED + corr;

                left  = clamp(left,  0, MAX_SPEED);
                right = clamp(right, 0, MAX_SPEED);

                motor_set(left, right);
                break;
            }

            case ROBOT_STOP:
                motor_stop();
                break;

            case ROBOT_ERROR:
            default:
                motor_stop();
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

