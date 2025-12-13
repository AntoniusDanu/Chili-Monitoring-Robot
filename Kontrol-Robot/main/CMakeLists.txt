idf_component_register(SRCS
    "main.c"
    "motor.c"
    "linefollow.c"
    "hcsr.c"
    "dht_task.c"
    "robot_state.c"
    "qtr.c"
    "wifi_http.c"
    INCLUDE_DIRS "."
    REQUIRES dht esp_wifi esp_event esp_netif nvs_flash esp_http_client esp_timer)
