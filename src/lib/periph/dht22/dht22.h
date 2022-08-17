#include <stddef.h>

#include "core/hardware.h"

typedef struct {
  int16_t temp_c_tenths;         // tempereature in units of 0.1 degrees C
  uint16_t humidity_pct_tenths;  // humidity in units of 0.1 percent
} dht22_data_t;

// Returns true if data is valid, false if not.
bool dht22_read(gpio_pin_t pin, dht22_data_t *d);
