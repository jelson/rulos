#include <stdio.h>
#include <string.h>

#include "flash_fixture.h"
#include "pulsegen.h"
#include "scpi.h"
#include "usb_fixture.h"

// Fake pulsegen state: the setters record, the getters read back, and apply order is logged so
// the load path's dependency ordering is checked.
typedef struct {
  uint64_t period_ps, width_ps, delay_ps, burst_rep_ps;
  uint32_t burst_ncyc;
  bool on, burst_on;
} fake_chan_t;
static fake_chan_t chan[NUM_CHANNELS];
static bool sync_mode;
static char apply_log[256];

static void note(char what) {
  size_t n = strlen(apply_log);
  if (n + 1 < sizeof(apply_log)) {
    apply_log[n] = what;
    apply_log[n + 1] = 0;
  }
}
const char *pulsegen_set_mode(bool sync) {
  sync_mode = sync;
  note('M');
  return NULL;
}
const char *pulsegen_set_state(int ch, bool on) {
  chan[ch].on = on;
  note('O');
  return NULL;
}
const char *pulsegen_set_period_ps(int ch, uint64_t ps) {
  chan[ch].period_ps = ps;
  note('P');
  return NULL;
}
const char *pulsegen_set_width_ps(int ch, uint64_t ps) {
  chan[ch].width_ps = ps;
  note('W');
  return NULL;
}
const char *pulsegen_set_delay_ps(int ch, uint64_t ps) {
  chan[ch].delay_ps = ps;
  note('D');
  return NULL;
}
const char *pulsegen_set_burst_state(int ch, bool on) {
  chan[ch].burst_on = on;
  note('B');
  return NULL;
}
const char *pulsegen_set_burst_ncyc(int ch, uint32_t ncyc) {
  chan[ch].burst_ncyc = ncyc;
  note('N');
  return NULL;
}
const char *pulsegen_set_burst_period_ps(int ch, uint64_t ps) {
  chan[ch].burst_rep_ps = ps;
  note('R');
  return NULL;
}
bool pulsegen_get_mode_sync(void) {
  return sync_mode;
}
const char *pulsegen_mode_str(void) {
  return sync_mode ? "SYNC" : "ASYNC";
}
bool pulsegen_get_state(int ch) {
  return chan[ch].on;
}
uint64_t pulsegen_get_period_ps(int ch) {
  return chan[ch].period_ps;
}
uint64_t pulsegen_get_width_ps(int ch) {
  return chan[ch].width_ps;
}
uint64_t pulsegen_get_delay_ps(int ch) {
  return chan[ch].delay_ps;
}
bool pulsegen_get_burst_state(int ch) {
  return chan[ch].burst_on;
}
uint32_t pulsegen_get_burst_ncyc(int ch) {
  return chan[ch].burst_ncyc;
}
uint64_t pulsegen_get_burst_period_ps(int ch) {
  return chan[ch].burst_rep_ps;
}
void pulsegen_apply_defaults(void) {
  sync_mode = false;
  memset(chan, 0, sizeof(chan));
  for (int i = 0; i < NUM_CHANNELS; i++) {
    chan[i].burst_ncyc = 1;
    chan[i].burst_rep_ps = 1000000000000ULL;
  }
  note('*');
}

static void configure_nondefault(void) {
  pulsegen_set_mode(true);
  pulsegen_set_period_ps(2, 1000000000ULL);
  pulsegen_set_width_ps(2, 5000000ULL);
  pulsegen_set_delay_ps(2, 250ULL);
  pulsegen_set_burst_ncyc(2, 7);
  pulsegen_set_burst_period_ps(2, 3000000000000ULL);
  pulsegen_set_burst_state(2, true);
  pulsegen_set_state(2, true);
}

static void assert_nondefault_restored(void) {
  assert(sync_mode);
  assert(chan[2].period_ps == 1000000000ULL && chan[2].width_ps == 5000000ULL);
  assert(chan[2].delay_ps == 250ULL && chan[2].burst_ncyc == 7);
  assert(chan[2].burst_rep_ps == 3000000000000ULL && chan[2].burst_on && chan[2].on);
  assert(!chan[0].on && chan[0].period_ps == 0);
}

static void test_load_order_and_roundtrip(void) {
  reset_flash();
  pulsegen_config_load();
  assert(!sync_mode && !chan[0].on && chan[0].burst_ncyc == 1);
  configure_nondefault();
  assert(pulsegen_config_save());
  unsigned writes = erase_calls;
  assert(pulsegen_config_save());  // unchanged: no flash write
  assert(erase_calls == writes);

  reboot_backend();
  memset(apply_log, 0, sizeof(apply_log));
  pulsegen_config_load();
  assert_nondefault_restored();
  // Defaults first, then mode, then each channel's period before width/delay and burst
  // parameters before burst state; every output state comes last.
  assert(apply_log[0] == '*' && apply_log[1] == 'M');
  assert(strcmp(apply_log + 2, "PWDNRBPWDNRBPWDNRBPWDNRBOOOO") == 0);
}

static void test_save_failure_retry(void) {
  for (unsigned failure = 0; failure < 4; failure++) {
    reset_flash();
    pulsegen_config_load();
    assert(pulsegen_config_save());
    configure_nondefault();
    fail_unlock = failure == 0;
    fail_erase = failure == 1;
    fail_program_call = failure == 2 ? program_calls + 1 : 0;
    fail_lock = failure == 3;
    assert(!pulsegen_config_save());
    fail_unlock = fail_erase = fail_lock = false;
    fail_program_call = 0;
    assert(pulsegen_config_save());
    reboot_backend();
    pulsegen_config_load();
    assert_nondefault_restored();
  }
}

static void test_reset_persists_defaults(void) {
  reset_flash();
  pulsegen_config_load();
  configure_nondefault();
  assert(pulsegen_config_save());
  fail_erase = true;
  assert(!pulsegen_reset_all());
  assert(!sync_mode && !chan[2].on);  // defaults applied even though persisting failed
  reboot_backend();
  pulsegen_config_load();
  assert_nondefault_restored();  // the old block survived the failed write
  fail_erase = false;
  assert(pulsegen_reset_all());
  reboot_backend();
  pulsegen_config_load();
  assert(!sync_mode && !chan[2].on && chan[2].period_ps == 0);
}

static void command_error(const char *command, const char *expected) {
  assert(usb_test_receive(command, strlen(command)));
  usb_test_run_tasks();
  assert(usb_test_complete());
  assert(strcmp(usb_test_last_tx(), expected) == 0);
  usb_test_run_tasks();
}

static void test_scpi(void) {
  reset_flash();
  pulsegen_config_load();
  pulsegen_scpi_init();
  usb_test_dtr(true);
  usb_test_run_tasks();
  configure_nondefault();
  fail_erase = true;
  command_error("CONF:SAVE\nSYST:ERR?\n", "-240,\"Configuration save failed\"\n");
  fail_erase = false;
  command_error("CONF:SAVE\nSYST:ERR?\n", "0,\"No error\"\n");
  reboot_backend();
  pulsegen_config_load();
  assert_nondefault_restored();
  fail_erase = true;
  command_error("*RST\nSYST:ERR?\n", "-240,\"Configuration save failed\"\n");
  assert(!chan[2].on);
  fail_erase = false;
  command_error("*RST\nSYST:ERR?\n", "0,\"No error\"\n");
  reboot_backend();
  pulsegen_config_load();
  assert(!chan[2].on && chan[2].period_ps == 0);
}

int main(void) {
  test_load_order_and_roundtrip();
  test_save_failure_retry();
  test_reset_persists_defaults();
  test_scpi();
  puts("pulsegen configuration: PASS");
  return 0;
}
