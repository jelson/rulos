#pragma once

#include <stdint.h>

extern uint32_t test_primask;
#define __get_PRIMASK() test_primask
#define __disable_irq() (test_primask = 1)
#define __enable_irq()  (test_primask = 0)
