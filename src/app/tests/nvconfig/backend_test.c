#include <stdio.h>

#include "flash_fixture.h"

static const uint8_t old_data[32] = {1, 2, 3, 4};
static const uint8_t new_data[32] = {5, 6, 7, 8};

static void assert_loaded(const uint8_t *expected) {
  uint8_t actual[sizeof(old_data)] = {0};
  reboot_backend();
  assert(nvconfig_load(actual, sizeof(actual), 1));
  assert(memcmp(actual, expected, sizeof(actual)) == 0);
}

static void test_invalid_requests(void) {
  reset_flash();
  uint8_t oversized[NVCONFIG_MAX_PAYLOAD + 1] = {0};
  assert(!nvconfig_save(NULL, sizeof(old_data), 1));
  assert(!nvconfig_save(oversized, sizeof(oversized), 1));
  assert(unlock_calls == 0 && erase_calls == 0 && program_calls == 0);
}

static void test_flash_failures(void) {
  for (unsigned failure = 0; failure < 5; failure++) {
    reset_flash();
    // Save-before-load must still discover the active slot correctly.
    assert(nvconfig_save(old_data, sizeof(old_data), 1));
    uint8_t old_slot[FLASH_SECTOR_SIZE];
    memcpy(old_slot, _nvconfig_base, sizeof(old_slot));
    unsigned before_program = program_calls;
    switch (failure) {
      case 0:
        fail_unlock = true;
        break;
      case 1:
        fail_erase = true;
        break;
      case 2:
        fail_program_call = program_calls + 1;
        break;
      case 3:
        fail_program_call = program_calls + NV_IMG(sizeof(new_data)) / 16;
        break;
      case 4:
        fail_lock = true;
        break;
    }
    assert(!nvconfig_save(new_data, sizeof(new_data), 1));
    assert(nv_active == 0 && nv_active_seq == 1);
    assert(memcmp(_nvconfig_base, old_slot, sizeof(old_slot)) == 0);
    if (failure < 2) {
      assert(program_calls == before_program);
    }
    if (failure != 4) {
      assert(!flash_unlocked);
      assert_loaded(old_data);
    }

    fail_unlock = fail_erase = fail_lock = false;
    fail_program_call = 0;
    assert(nvconfig_save(new_data, sizeof(new_data), 1));
    assert(!flash_unlocked);
    assert_loaded(new_data);
  }
}

static void test_load_validation(void) {
  reset_flash();
  uint8_t out[sizeof(old_data)];
  memset(out, 0x5a, sizeof(out));
  assert(!nvconfig_load(out, sizeof(out), 1));
  assert(out[0] == 0x5a);
  assert(nvconfig_save(old_data, sizeof(old_data), 1));
  assert(!nvconfig_load(out, sizeof(out), 2));
  assert(out[0] == 0x5a);
  assert(!nvconfig_load(out, sizeof(out) - 1, 1));
  assert(out[0] == 0x5a);
  _nvconfig_base[sizeof(nvconfig_hdr_t)] ^= 1;
  assert(!nvconfig_load(out, sizeof(out), 1));
  assert(out[0] == 0x5a);
}

int main(void) {
  test_invalid_requests();
  if (nv_region() < sizeof(_nvconfig_base) || NV_IMG(sizeof(old_data)) > FLASH_SECTOR_SIZE) {
    uint8_t out[sizeof(old_data)] = {0};
    assert(!nvconfig_load(out, sizeof(out), 1));
    assert(!nvconfig_save(old_data, sizeof(old_data), 1));
    assert(unlock_calls == 0 && erase_calls == 0 && program_calls == 0);
    puts("nvconfig invalid flash geometry: PASS");
    return 0;
  }
  test_flash_failures();
  test_load_validation();
  puts("nvconfig backend: PASS");
  return 0;
}
