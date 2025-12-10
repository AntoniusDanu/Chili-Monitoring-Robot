#include <string.h>
#include <stdio.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_camera.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_http_client.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include "esp_http_server.h"
#include "esp_ota_ops.h"

#define WIFI_SSID       "vivo Y33S"//"BRT Juken"
#define WIFI_PASS       "arduinouno"//"A1b2c3d4e5"
#define TAG "ESP32-CAM"
#define LED_GPIO        GPIO_NUM_4
//#define PIT_ID          4  // Ganti sesuai ID PIT (0=P1, 1=P2, dst)
#define BACKEND_URL     "http://192.168.57.84:8000/chili/upload"      

#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM     5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0


static void init_led(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(LED_GPIO, 1); 
}

static void blink_led_success(void) {
    gpio_set_level(LED_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LED_GPIO, 0);
}

static void blink_led_error(void) {
    for (int i = 0; i < 2; ++i) {
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

static void test_dns_resolution() {
    struct hostent *he = gethostbyname("167.172.79.82");
    if (he == NULL) {
    } else {
        ESP_LOGI(TAG, " DNS lookup success. IP: %s", inet_ntoa(*(struct in_addr*)he->h_addr));
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi terputus. Mencoba reconnect...");
        esp_wifi_connect();
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}
//----Default IP-------
static void connect_wifi(void) {
    wifi_event_group = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();

    // Pastikan DHCP aktif
    esp_netif_dhcpc_stop(netif);  // optional safe reset
    esp_netif_dhcpc_start(netif);

    // WiFi init
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    // Tunggu koneksi WiFi
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(10000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected (DHCP)");
        blink_led_success();
    } else {
        ESP_LOGE(TAG, "WiFi connection failed");
        blink_led_error();
    }
}
//-----SET IP STATIS ---------
/*
static void connect_wifi(void) {
    wifi_event_group = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();

    // IP statis
    esp_netif_dhcpc_stop(netif);  // stop DHCP

    // IP, gateway, dan netmask 
    ip4_addr_t ip, gw, netmask;
    ip4addr_aton("192.168.4.5", &ip);      
    ip4addr_aton("192.168.4.1", &gw);     
    ip4addr_aton("255.255.255.0", &netmask);  
     
    esp_netif_ip_info_t ip_info;
    ip_info.ip.addr = ip.addr;
    ip_info.gw.addr = gw.addr;
    ip_info.netmask.addr = netmask.addr;

    esp_netif_set_ip_info(netif, &ip_info); 

    // DNS
    esp_netif_dns_info_t dns;
    dns.ip.type = IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = inet_addr("8.8.8.8");
    esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns);

    // WiFi init
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .pmf_cfg = {
                 .capable = true,
                 .required = false,  // <= pastikan ini false
        },
      },
    };

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    // Tunggu koneksi WiFi
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(10000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, " WiFi connected.");
        ESP_LOGI(TAG, " Static IP Address: " IPSTR, IP2STR(&ip_info.ip));
        gpio_set_level(LED_GPIO, 0); 
    } else {
        ESP_LOGE(TAG, " WiFi connection failed");
        blink_led_error();
    }
}*/

esp_err_t upload_image(uint8_t *image_buf, size_t image_len) {
    const char *url = BACKEND_URL;
    ESP_LOGI(TAG, "Upload URL: %s", url);

    const char *boundary = "----esp32boundary";
    char start_part[256];
    char end_part[64];

    snprintf(start_part, sizeof(start_part),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"image.jpg\"\r\n"
        "Content-Type: image/jpeg\r\n\r\n", boundary);

    snprintf(end_part, sizeof(end_part), "\r\n--%s--\r\n", boundary);

    size_t total_len = strlen(start_part) + image_len + strlen(end_part);

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 10000,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    char content_type[64];
    snprintf(content_type, sizeof(content_type),
        "multipart/form-data; boundary=%s", boundary);

    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_http_client_set_method(client, HTTP_METHOD_POST);

    esp_err_t err = esp_http_client_open(client, total_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    esp_http_client_write(client, start_part, strlen(start_part));
    esp_http_client_write(client, (char *)image_buf, image_len);
    esp_http_client_write(client, end_part, strlen(end_part));

    err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP perform failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Upload selesai, Status = %d", esp_http_client_get_status_code(client));
    }

    esp_http_client_cleanup(client);
    return err;
}

static esp_err_t init_camera(void) {
    camera_config_t config = {
        .pin_pwdn       = PWDN_GPIO_NUM,
        .pin_reset      = RESET_GPIO_NUM,
        .pin_xclk       = XCLK_GPIO_NUM,
        .pin_sccb_sda   = SIOD_GPIO_NUM,
        .pin_sccb_scl   = SIOC_GPIO_NUM,
        .pin_d7         = Y9_GPIO_NUM,
        .pin_d6         = Y8_GPIO_NUM,
        .pin_d5         = Y7_GPIO_NUM,
        .pin_d4         = Y6_GPIO_NUM,
        .pin_d3         = Y5_GPIO_NUM,
        .pin_d2         = Y4_GPIO_NUM,
        .pin_d1         = Y3_GPIO_NUM,
        .pin_d0         = Y2_GPIO_NUM,
        .pin_vsync      = VSYNC_GPIO_NUM,
        .pin_href       = HREF_GPIO_NUM,
        .pin_pclk       = PCLK_GPIO_NUM,
        .xclk_freq_hz   = 20000000,
        .ledc_timer     = LEDC_TIMER_0,
        .ledc_channel   = LEDC_CHANNEL_0,
        .pixel_format   = PIXFORMAT_JPEG,
        .frame_size     = FRAMESIZE_UXGA,
        .jpeg_quality   = 12,
        .fb_count       = 2,
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed: %s", esp_err_to_name(err));
        return err;
    }
    //---Konfigurasi lensa camera----
    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 0);
    s->set_hmirror(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, 0);
    return ESP_OK;
}

static void capture_and_upload_task(void *pvParameters) {
    while (1) {

        blink_led_success();                  // <--- Tambahan
        vTaskDelay(pdMS_TO_TICKS(200));       // Delay kecil biar lampu sempat menyala

        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Failed to capture image");
            blink_led_error();
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        if (upload_image(fb->buf, fb->len) == ESP_OK) {
            ESP_LOGI(TAG, "Upload sukses");
            blink_led_success();
        } else {
            ESP_LOGE(TAG, "Upload gagal");
            blink_led_error();
        }

        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}


/*
//----ESP kirim status Heartbeat (PING) ke backend-------
static void task_heartbeat(void *pvParameters) {
    while (1) {
        esp_http_client_config_t config = {
            .url = "h,
            .method = HTTP_METHOD_POST,
            .timeout_ms = 5000,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");

        char post_data[32];
        snprintf(post_data, sizeof(post_data), "pit_id=PIT%d", PIT_ID + 1);  // +1 karena PIT_ID = 0 artinya PIT1

        esp_http_client_set_post_field(client, post_data, strlen(post_data));

        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            ESP_LOGI("HEARTBEAT", "Terkirim: %d", esp_http_client_get_status_code(client));
        } else {
            ESP_LOGW("HEARTBEAT", "Gagal: %s", esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);
        vTaskDelay(pdMS_TO_TICKS(30000)); 
    }
}*/
//----OTA WEBSERVER HANDLER------
esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
}

esp_err_t upload_post_handler(httpd_req_t *req) {
    esp_ota_handle_t ota_handle;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) return ESP_FAIL;

    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) return err;

    char buf[1024];
    int remaining = req->content_len;

    while (remaining > 0) {
        int received = httpd_req_recv(req, buf, sizeof(buf));
        if (received <= 0) return ESP_FAIL;
        err = esp_ota_write(ota_handle, buf, received);
        if (err != ESP_OK) return err;
        remaining -= received;
    }

    if (esp_ota_end(ota_handle) == ESP_OK) {
        err = esp_ota_set_boot_partition(update_partition);
        if (err == ESP_OK) {
            httpd_resp_sendstr(req, "Upload sukses. Restarting...\n");
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        } else httpd_resp_send_500(req);
    } else httpd_resp_sendstr(req, "OTA end failed.");

    return ESP_OK;
}

void start_ota_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = {
            .uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);

        httpd_uri_t upload_uri = {
            .uri = "/upload", .method = HTTP_POST, .handler = upload_post_handler, .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &upload_uri);
    }
}

void ota_server_task(void *pvParameters) {
    start_ota_server();
    vTaskDelete(NULL);  // Hapus task setelah server dijalankan
}
void app_main(void) {
    nvs_flash_init();
    init_led();
    connect_wifi();
    test_dns_resolution();
    init_camera();
    xTaskCreate(capture_and_upload_task, "capture_and_upload_task", 8192, NULL, 3, NULL);
    //xTaskCreate(task_heartbeat, "task_heartbeat", 4096, NULL, 2, NULL);
    xTaskCreate(ota_server_task, "ota_server_task", 4096, NULL, 1, NULL);
}
