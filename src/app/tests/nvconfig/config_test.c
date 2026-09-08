#include <stdio.h>

#include "flash_fixture.h"
#include "scpi.h"
#include "timestamper.h"
#include "usb_fixture.h"

static timestamper_slope_t slopes[NUM_CHANNELS];
static uint32_t dividers[NUM_CHANNELS];
static bool stream_enabled;
static timestamper_format_t format;
static bool serial_enabled;
static uint32_t serial_baud;
static unsigned clear_calls;

void timestamper_set_slope(int ch, timestamper_slope_t slope) {
  slopes[ch] = slope;
}
timestamper_slope_t timestamper_get_slope(int ch) {
  return slopes[ch];
}
void timestamper_set_divider(int ch, uint32_t divider) {
  dividers[ch] = divider;
}
uint32_t timestamper_get_divider(int ch) {
  return dividers[ch];
}
void timestamper_set_stream_enabled(bool enabled) {
  stream_enabled = enabled;
}
bool timestamper_get_stream_enabled(void) {
  return stream_enabled;
}
void timestamper_set_format(timestamper_format_t fmt) {
  format = fmt;
}
timestamper_format_t timestamper_get_format(void) {
  return format;
}
void timestamper_serial_set_enabled(bool enabled) {
  serial_enabled = enabled;
}
bool timestamper_serial_get_enabled(void) {
  return serial_enabled;
}
void timestamper_serial_set_baud(uint32_t baud) {
  serial_baud = baud;
}
uint32_t timestamper_serial_get_baud(void) {
  return serial_baud;
}
void timestamper_discard_pending(void) {
  clear_calls++;
}

static void test_save_failure_retry(void) {
  for (unsigned failure = 0; failure < 4; failure++) {
    reset_flash();
    timestamper_config_load();
    assert(dividers[0] == 1 && slopes[0] == TIMESTAMPER_SLOPE_RISING);
    assert(timestamper_config_save());
    // Blank flash must not be treated as already containing the defaults.
    assert(erase_calls == 1);
    assert(timestamper_config_save());
    assert(erase_calls == 1);

    timestamper_set_slope(0, TIMESTAMPER_SLOPE_FALLING);
    timestamper_set_divider(0, 7);
    fail_unlock = failure == 0;
    fail_erase = failure == 1;
    fail_program_call = failure == 2 ? program_calls + 1 : 0;
    fail_lock = failure == 3;
    assert(!timestamper_config_save());
    assert(test_primask == 0);
    if (failure != 3) {
      unsigned before_retry = unlock_calls;
      if (failure == 2) {
        fail_program_call = program_calls + 1;
      }
      assert(!timestamper_config_save());
      assert(unlock_calls == before_retry + 1);
    }
    fail_unlock = fail_erase = fail_lock = false;
    fail_program_call = 0;
    assert(timestamper_config_save());
    unsigned writes = erase_calls;
    assert(timestamper_config_save());
    assert(erase_calls == writes);
    reboot_backend();
    timestamper_config_load();
    assert(dividers[0] == 7 && slopes[0] == TIMESTAMPER_SLOPE_FALLING);
  }
}

static void test_reset_failure_retry(void) {
  reset_flash();
  timestamper_config_load();
  timestamper_set_divider(0, 11);
  assert(timestamper_config_save());

  fail_erase = true;
  assert(!timestamper_reset_all());
  assert(stream_enabled && format == TIMESTAMPER_FORMAT_TEXT);
  assert(!serial_enabled && serial_baud == SERIAL_DEFAULT_BAUD);
  assert(clear_calls > 0 && dividers[0] == 1);
  reboot_backend();
  timestamper_config_load();
  assert(dividers[0] == 11);
  fail_erase = false;
  assert(timestamper_reset_all());
  reboot_backend();
  timestamper_config_load();
  assert(dividers[0] == 1 && slopes[0] == TIMESTAMPER_SLOPE_RISING);
}

static void test_irq_mask_preserved(void) {
  reset_flash();
  timestamper_config_load();
  test_primask = 1;
  assert(timestamper_config_save());
  assert(test_primask == 1);
  timestamper_set_divider(0, 7);
  fail_erase = true;
  assert(!timestamper_config_save());
  assert(test_primask == 1);
}

static void test_uncertain_write_revert(void) {
  reset_flash();
  timestamper_config_load();
  assert(timestamper_config_save());
  timestamper_set_divider(0, 7);
  fail_lock = true;
  assert(!timestamper_config_save());
  fail_lock = false;
  // The new slot is already valid even though locking failed. Reverting to the previous settings
  // must not be skipped based on the old saved mirror.
  timestamper_set_divider(0, 1);
  unsigned writes = erase_calls;
  assert(timestamper_config_save());
  assert(erase_calls == writes + 1);
  reboot_backend();
  timestamper_config_load();
  assert(dividers[0] == 1);
}

static void command_error(const char *command, const char *expected) {
  assert(usb_test_receive(command, strlen(command)));
  usb_test_run_tasks();
  assert(usb_test_complete());
  assert(strcmp(usb_test_last_tx(), expected) == 0);
  usb_test_run_tasks();
}

static void test_scpi_errors(void) {
  reset_flash();
  timestamper_config_load();
  timestamper_scpi_init(NULL);
  usb_test_dtr(true);
  usb_test_run_tasks();
  for (unsigned failure = 0; failure < 2; failure++) {
    timestamper_set_divider(0, 7 + failure);
    fail_erase = failure == 0;
    fail_program_call = failure == 1 ? program_calls + 1 : 0;
    command_error("CONF:SAVE\nSYST:ERR?\n", "-240,\"Configuration save failed\"\n");
    fail_erase = false;
    fail_program_call = 0;
    command_error("CONF:SAVE\nSYST:ERR?\n", "0,\"No error\"\n");
    reboot_backend();
    timestamper_config_load();
    assert(dividers[0] == 7 + failure);
  }

  fail_erase = true;
  command_error("*RST\nSYST:ERR?\n", "-240,\"Configuration save failed\"\n");
  assert(dividers[0] == 1);
  fail_erase = false;
  command_error("*RST\nSYST:ERR?\n", "0,\"No error\"\n");
  reboot_backend();
  timestamper_config_load();
  assert(dividers[0] == 1);
}

int main(void) {
  test_save_failure_retry();
  test_reset_failure_retry();
  test_irq_mask_preserved();
  test_uncertain_write_revert();
  test_scpi_errors();
  puts("timestamper configuration: PASS");
  return 0;
}
