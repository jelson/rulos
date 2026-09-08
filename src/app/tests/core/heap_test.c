#include "core/heap.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void log_assert(const char *file, unsigned long line) {
  fprintf(stderr, "%s:%lu: assertion failed\n", file, line);
  abort();
}

static void entry(void *data) {
  (void)data;
}

// Insert keys in a scrambled order, then verify pops come out in later_than
// order, including keys that straddle the 32-bit rollover.
static void check_order(const Time *keys, int n) {
  Heap heap;
  heap_init(&heap);
  for (int i = 0; i < n; i++) {
    int j = (i * 7 + 3) % n;
    assert(heap_insert(&heap, keys[j], entry, (void *)(uintptr_t)keys[j]) == i + 1);
  }
  Time prev = 0;
  for (int i = 0; i < n; i++) {
    Time key;
    ActivationRecord act;
    assert(heap_peek(&heap, &key, &act) == 0);
    assert(act.func == entry);
    assert((Time)(uintptr_t)act.data == key);
    if (i > 0) {
      assert(!later_than(prev, key));
    }
    prev = key;
    heap_pop(&heap);
  }
  assert(heap_peek(&heap, &prev, (ActivationRecord[]){{0}}) == -1);
}

int main(void) {
  Time sequential[SCHEDULER_CAPACITY];
  Time rollover[SCHEDULER_CAPACITY];
  Time duplicates[SCHEDULER_CAPACITY];
  for (int i = 0; i < SCHEDULER_CAPACITY; i++) {
    sequential[i] = 1000 * i;
    rollover[i] = UINT32_MAX - 5000 + 1000 * i;
    duplicates[i] = 1000 * (i / 3);
  }
  check_order(sequential, SCHEDULER_CAPACITY);
  check_order(rollover, SCHEDULER_CAPACITY);
  check_order(duplicates, SCHEDULER_CAPACITY);
  for (int n = 1; n <= 5; n++) {
    check_order(sequential, n);
  }
  puts("heap: insert/pop ordering across capacity, rollover, and duplicates passed");
}
