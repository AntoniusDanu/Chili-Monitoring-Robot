#include "hcsr.h"
#include "robot_state.h"
#include <string.h>

#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"

#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"

/* ===== PIN CONFIG ===== */
#define TRIG_PIN GPIO_NUM_12
#define ECHO_PIN GPIO_NUM_13

/* ===== PARAM ===== */
#define TIMEOUT_US         30000
#define STOP_CM            5
#define DETECT_CONFIRM     3
#define LOG_INTERVAL_US    1000000

static const char *TAG = "HCSR";

/* ===== EXTERNAL ===== */
extern SemaphoreHandle_t log_mutex;

/* ===== ESPNOW ===== */
static uint8_t CAM_MAC[6] = { 0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC }; // GANTI!

typedef enum {
    CMD_NONE = 0,
    CMD_TAKE_PICTURE,
    CMD_DONE
} cmd_t;

typedef struct {
    uint8_t  cmd;
    uint16_t pot;
} espnow_msg_t;

static SemaphoreHandle_t done_sem;

/* ===== INTERNAL ===== */
static int last_state   = ROBOT_RUN;
static int pot_counter  = 1;
static int detect_count = 0;

/* =========================================================
 *  ESPNOW CALLBACK
 * ========================================================= */
static void espnow_recv_cb(const esp_now_recv_info_t *info,
                           const uint8_t *data, int len)
{
    if (len != sizeof(espnow_msg_t)) return;

    espnow_msg_t msg;
    memcpy(&msg, data, sizeof(msg));

    if (msg.cmd == CMD_DONE) {
        ESP_LOGI(TAG, "DONE received from CAM");
        xSemaphoreGive(done_sem);
    }
}

/* =========================================================
 *  ESPNOW INIT
 * ========================================================= */
static void espnow_init(void)
{
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_now_init();
    esp_now_register_recv_cb(espnow_recv_cb);

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, CAM_MAC, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    done_sem = xSemaphoreCreateBinary();

    ESP_LOGI(TAG, "ESP-NOW ready");
}

/* =========================================================
 *  INIT HCSR
 * ========================================================= */
void hcsr_init(void)
{
    gpio_config_t io = {};

    io.pin_bit_mask = 1ULL << TRIG_PIN;
    io.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io);

    io.pin_bit_mask = 1ULL << ECHO_PIN;
    io.mode = GPIO_MODE_INPUT;
    gpio_config(&io);

    gpio_set_level(TRIG_PIN, 0);

    espnow_init();
}

/* =========================================================
 *  JANGAN DIUBAH
 * ========================================================= */
int hcsr_read_cm(void)
{
    int64_t t_start, t_end;

    gpio_set_level(TRIG_PIN, 0);
    esp_rom_delay_us(5);
    gpio_set_level(TRIG_PIN, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG_PIN, 0);

    int64_t t0 = esp_timer_get_time();
    while (gpio_get_level(ECHO_PIN) == 0)
        if (esp_timer_get_time() - t0 > TIMEOUT_US)
            return -1;

    t_start = esp_timer_get_time();
    while (gpio_get_level(ECHO_PIN) == 1)
        if (esp_timer_get_time() - t_start > TIMEOUT_US)
            return -1;

    t_end = esp_timer_get_time();

    int d = (int)((t_end - t_start) / 58);
    if (d < 2 || d > 400) return -2;

    return d;
}

/* =========================================================
 *  SAFE LOG (LAMA)
 * ========================================================= */
static void hcsr_log(int d)
{
#if HCSR_TEST_LOG
    if (log_mutex && xSemaphoreTake(log_mutex, pdMS_TO_TICKS(20)))
    {
        if (d < 0)
            ESP_LOGW(TAG, "Invalid (%d)", d);
        else
            ESP_LOGI(TAG, "Distance: %d cm", d);
        xSemaphoreGive(log_mutex);
    }
#endif
}

/* =========================================================
 *  TASK
 * ========================================================= */
void hcsr_task(void *pv)
{
    hcsr_init();
    vTaskDelay(pdMS_TO_TICKS(500));

    int64_t last_log = 0;

    while (1)
    {
        int d = hcsr_read_cm();
        int next_state = ROBOT_RUN;
        int64_t now = esp_timer_get_time();

        if (now - last_log > LOG_INTERVAL_US) {
            hcsr_log(d);
            last_log = now;
        }

        if (d > 0 && d < STOP_CM) {
            detect_count++;
            if (detect_count >= DETECT_CONFIRM)
                next_state = ROBOT_STOP;
        } else {
            detect_count = 0;
        }

        if (last_state == ROBOT_RUN && next_state == ROBOT_STOP)
        {
            espnow_msg_t msg = {
                .cmd = CMD_TAKE_PICTURE,
                .pot = pot_counter
            };

            esp_now_send(CAM_MAC, (uint8_t *)&msg, sizeof(msg));

            ESP_LOGI(TAG, "STOP → pot %d → TAKE_PICTURE", pot_counter);

            xSemaphoreTake(done_sem, portMAX_DELAY);

            pot_counter++;
            detect_count = 0;

            robot_state = ROBOT_RUN;
            last_state  = ROBOT_RUN;
            continue;
        }

        robot_state = next_state;
        last_state  = next_state;

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
