#pragma once
#include <stdint.h>

#define QTR_SENSOR_COUNT 8

void qtr_init(void);
void qtr_read_raw(uint32_t *values);
int  qtr_read_position(void);

