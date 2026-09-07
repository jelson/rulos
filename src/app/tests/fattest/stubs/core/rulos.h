#pragma once

#include "core/time.h"

Time clock_time_us(void);
void sd_test_idle(void);
static inline void sd_test_log(const char *format, ...) {
  (void)format;
}
#define LOG(...) sd_test_log(__VA_ARGS__)
#define __WFI()  sd_test_idle()
