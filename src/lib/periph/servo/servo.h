#pragma once

#include <stdint.h>

void servo_init();
void servo_set_pwm(uint16_t desired_position);
