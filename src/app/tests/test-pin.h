#pragma once

// TEST_PIN and matching gpio_* macros for test programs that toggle a
// scope-visible pin. On the simulator there's no GPIO hardware, so
// gpio_* compile to no-ops and TEST_PIN is a placeholder.

#ifdef SIM
#define TEST_PIN              0
#define gpio_make_output(pin) ((void)0)
#define gpio_set(pin)         ((void)0)
#define gpio_clr(pin)         ((void)0)
#else
#include "core/hardware.h"
#if defined(RULOS_ARM_STM32)
#define TEST_PIN GPIO_A0
#elif defined(RULOS_ARM_NXP)
#define TEST_PIN GPIO0_00
#elif defined(RULOS_AVR)
#define TEST_PIN GPIO_B3
#elif defined(RULOS_ESP32)
#define TEST_PIN GPIO_2
#else
#error "No test pin defined"
#include <stophere>
#endif
#endif
