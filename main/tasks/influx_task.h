#pragma once

#include "influx.h"

void influx_task_set_temperature(float temp, float temp2);
void influx_task_set_pwr(float vin, float iin, float pin, float vout, float iout, float pout);

void influx_task(void *pvParameters);