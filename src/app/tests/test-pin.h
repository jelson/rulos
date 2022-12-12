#if defined(RULOS_ARM_STM32)
#define TEST_PIN GPIO_A10
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
