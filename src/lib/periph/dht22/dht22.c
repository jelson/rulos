#include "dht22.h"

#include <stdbool.h>

#include "core/hardware.h"
#include "core/rulos.h"

#define DHT22_BUFLEN 5
#define NUM_BITS     ((DHT22_BUFLEN)*8)

#define MAX_CYCLES    (10000)
#define DHT22_TIMEOUT ((uint32_t)(1 << 30))

static uint32_t expect_pulse(gpio_pin_t pin, bool level) {
  uint32_t count = 0;
  while (gpio_is_set(pin) == level) {
    if (count++ >= MAX_CYCLES) {
      return DHT22_TIMEOUT;
    }
  }
  return count;
}

bool dht22_read(gpio_pin_t pin, dht22_data_t *dht22_data) {
  // Reset 40 bits of received data to zero.
  uint8_t data[DHT22_BUFLEN];
  memset(data, 0, DHT22_BUFLEN);

  // Go into high impedence state to let pull-up raise data line level and start
  // the reading process.
  gpio_make_input_enable_pullup(pin);
  hal_delay_ms(1);

  // Now assert low. Datasheet says "at least 1ms"
  gpio_make_output(pin);
  gpio_clr(pin);
  hal_delay_ms(2);

  // Make pin an input again and wait for it to go high
  gpio_make_input_enable_pullup(pin);

  // Timing critical code starts here; disable interrupts
  rulos_irq_state_t old_interrupts = hal_start_atomic();

  // First wait for the pullup to take effect
  for (int i = 0; i < MAX_CYCLES; i++) {
    if (gpio_is_clr(pin)) {
      break;
    }
  }

  // Wait for DHT22 to pull the pin low
  for (int i = 0; i < MAX_CYCLES; i++) {
    if (gpio_is_set(pin)) {
      break;
    }
  }

  // First expect a low signal for ~80 microseconds followed by a high signal
  // for ~80 microseconds again.
  if (expect_pulse(pin, 0) == DHT22_TIMEOUT) {
    LOG("DHT22: timeout waiting for start signal low pulse");
    return false;
  }
  if (expect_pulse(pin, 1) == DHT22_TIMEOUT) {
    LOG("DHT22: timeout waiting for start signal high pulse");
    return false;
  }

  // Now read the 40 bits sent by the sensor.  Each bit is sent as a 50
  // microsecond low pulse followed by a variable length high pulse.  If the
  // high pulse is ~28 microseconds then it's a 0 and if it's ~70 microseconds
  // then it's a 1.  We measure the cycle count of the initial 50us low pulse
  // and use that to compare to the cycle count of the high pulse to determine
  // if the bit is a 0 (high state cycle count < low state cycle count), or a 1
  // (high state cycle count > low state cycle count). Note that for speed all
  // the pulses are read into a array and then examined in a later step.
  uint32_t cycles[NUM_BITS * 2];
  for (int i = 0; i < NUM_BITS * 2; i += 2) {
    cycles[i] = expect_pulse(pin, 0);
    cycles[i + 1] = expect_pulse(pin, 1);
  }
  hal_end_atomic(old_interrupts);
  // Timing critical code is now complete.

  // Inspect pulses and determine which ones are 0 (high state cycle count < low
  // state cycle count), or 1 (high state cycle count > low state cycle count).
  for (int i = 0; i < NUM_BITS; ++i) {
    uint32_t lowCycles = cycles[2 * i];
    uint32_t highCycles = cycles[2 * i + 1];
    // LOG("low %4d  |  high %4d", lowCycles, highCycles);
    if ((lowCycles == DHT22_TIMEOUT) || (highCycles == DHT22_TIMEOUT)) {
      LOG("DHT22: timeout waiting for pulse");
      return false;
    }
    data[i / 8] <<= 1;
    // Now compare the low and high cycle times to see if the bit is a 0 or 1.
    if (highCycles > lowCycles) {
      // High cycles are greater than 50us low cycle count, must be a 1.
      data[i / 8] |= 1;
    }
    // Else high cycles are less than (or equal to, a weird case) the 50us low
    // cycle count so this must be a zero.  Nothing needs to be changed in the
    // stored data.
  }

#if 0
  LOG("Got: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x", data[0], data[1], data[2],
      data[3], data[4]);
#endif

  if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
    LOG("DHT22: Checksum failure, got 0x%02x",
        data[0] + data[1] + data[2] + data[3]);
    return false;
  }

  // Decode
  dht22_data->humidity_pct_tenths = ((uint16_t)data[0]) << 8 | data[1];
  dht22_data->temp_c_tenths = ((int16_t)(data[2] & 0x7F)) << 8 | data[3];
  if (data[2] & 0x80) {
    dht22_data->temp_c_tenths = -dht22_data->temp_c_tenths;
  }

  return true;
}
