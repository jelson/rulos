#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/clock.h"

static jmp_buf finished;
static Time expected;
static clock_handler_t tick;
static void *tick_data;

rulos_irq_state_t hal_start_atomic(void) {
  return 0;
}
void hal_end_atomic(rulos_irq_state_t state) {
  (void)state;
}
uint32_t hal_start_clock_us(uint32_t us, clock_handler_t handler, void *data, uint8_t timer) {
  (void)timer;
  tick = handler;
  tick_data = data;
  return us;
}
void hal_idle(void) {
  tick(tick_data);
}
uint32_t hal_elapsed_us_in_tick(void) {
  return 0;
}
bool hal_clock_interrupt_is_pending(void) {
  return false;
}
void log_assert(const char *file, unsigned long line) {
  fprintf(stderr, "%s:%lu: assertion failed\n", file, line);
  abort();
}

static void activation(void *data) {
  (void)data;
  assert(clock_time_us() == expected);
  longjmp(finished, 1);
}

int main(void) {
  const Time values[] = {0, 1, 1000, 0x7fffffff, 0x80000000, UINT32_MAX};
  for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
    Time t = values[i];
    assert(!later_than(t, t));
    assert(later_than_or_eq(t, t));
    assert(later_than(t + 1, t));
    assert(!later_than(t, t + 1));
    assert(later_than(t + 0x7fffffff, t));
    assert(!later_than(t + 0x80000000, t));
  }
  for (unsigned offset = 0; offset <= 10000; offset += 10000) {
    init_clock(10000, TIMER1);
    expected = clock_time_us() + offset;
    schedule_absolute(expected, activation, NULL);
    if (!setjmp(finished)) {
      scheduler_run();
    }
  }
  puts("time: equality, rollover, and scheduler deadlines passed");
}
