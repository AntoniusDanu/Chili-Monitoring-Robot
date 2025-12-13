#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* ===== CONFIG ===== */
#define HCSR_TEST_LOG   1   // 1 = print log, 0 = silent

/* ===== API ===== */
void hcsr_init(void);
int  hcsr_read_cm(void);
void hcsr_task(void *pv);

/* Mutex log global (di-define di main.c) */
extern SemaphoreHandle_t log_mutex;

