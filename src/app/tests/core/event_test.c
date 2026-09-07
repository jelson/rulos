#include "core/event.h"

#include <assert.h>
#include <stdio.h>

static unsigned scheduled;
static ActivationFuncPtr last_func;
static void *last_data;

void schedule_now(ActivationFuncPtr func, void *data) {
  scheduled++;
  last_func = func;
  last_data = data;
}

static void callback(void *data) {
  (void)data;
}

int main(void) {
  for (unsigned auto_reset = 0; auto_reset <= 1; auto_reset++) {
    for (unsigned wait_first = 0; wait_first <= 1; wait_first++) {
      Event evt;
      scheduled = 0;
      event_init(&evt, auto_reset);
      if (wait_first) {
        event_wait(&evt, callback, &evt);
      }
      event_signal(&evt);
      if (!wait_first) {
        assert(event_is_signaled(&evt));
        event_wait(&evt, callback, &evt);
      }
      assert(scheduled == 1);
      assert(last_func == callback && last_data == &evt);
      assert(event_is_signaled(&evt) == !auto_reset);
      event_wait(&evt, callback, &evt);
      assert(scheduled == (auto_reset ? 1u : 2u));
      if (auto_reset) {
        event_signal(&evt);
        assert(scheduled == 2);
        assert(!event_is_signaled(&evt));
      }
      event_reset(&evt);
      assert(!event_is_signaled(&evt));
      event_wait(&evt, callback, &evt);
      assert(scheduled == 2);
      event_signal(&evt);
      assert(scheduled == 3);
    }
  }
  puts("event: manual/auto reset, signal/wait order, and reset passed");
}
