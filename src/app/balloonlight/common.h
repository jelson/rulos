#pragma once

#include <stdint.h>

#include "core/hardware.h"

#define LIGHT_EN GPIO_B7
#define ACCEL_INT GPIO_B5

// start accelerometer
void start_accel();
void read_accel_values(int16_t accel_out[3]);
