#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "usb_fixture.h"

static unsigned delivered;
static bool pause, resume_inside, reset_inside;
static const uint8_t *retained_data;

static void on_rx(usbd_cdc_state_t *cdc, void *user_data, const uint8_t *data, uint32_t len) {
  assert(len == 1 && (data[0] == 'x' || data[0] == 'y'));
  delivered++;
  retained_data = data;
  if (reset_inside) {
    reset_inside = false;
    usb_test_unplug();
    usb_test_replug();
    usb_test_dtr(true);
    assert(usb_test_receive("y", 1));
    assert(data[0] == 'x');
  }
  if (pause) {
    usbd_cdc_pause_rx(cdc);
  }
  if (resume_inside) {
    unsigned arms = usb_test_arm_count();
    usbd_cdc_resume_rx(cdc);
    assert(usb_test_arm_count() == arms);
  }
}

int main(void) {
  usbd_cdc_state_t cdc = {.rx_cb = on_rx};
  usbd_cdc_init(&cdc);
  usb_test_dtr(true);
  usb_test_run_tasks();

  assert(usb_test_receive("x", 1));
  assert(!usb_test_rx_armed() && delivered == 0);
  usb_test_run_tasks();
  assert(usb_test_rx_armed() && delivered == 1);

  pause = true;
  assert(usb_test_receive("x", 1));
  usb_test_run_tasks();
  assert(!usb_test_rx_armed() && delivered == 2);
  assert(!usb_test_receive("x", 1));
  usbd_cdc_resume_rx(&cdc);
  usbd_cdc_resume_rx(&cdc);
  assert(usb_test_rx_armed());

  resume_inside = true;
  assert(usb_test_receive("x", 1));
  usb_test_run_tasks();
  assert(usb_test_rx_armed() && delivered == 3);

  unsigned arms = usb_test_arm_count();
  assert(usb_test_receive("", 0));
  assert(usb_test_rx_armed() && usb_test_arm_count() == arms + 1);
  usb_test_run_tasks();
  assert(delivered == 3);

  // Pausing an already armed endpoint allows that one packet, then blocks
  // further rearming. A ZLP must respect that pause just like ordinary data.
  usbd_cdc_pause_rx(&cdc);
  assert(usb_test_receive("", 0));
  assert(!usb_test_rx_armed());
  usbd_cdc_resume_rx(&cdc);
  assert(usb_test_rx_armed());

  // A reset/reinitialization interrupt while the callback is running must
  // not allow a new packet to replace the buffer it is still reading.
  reset_inside = true;
  assert(usb_test_receive("x", 1));
  usb_test_run_tasks();
  assert(usb_test_rx_armed());
  reset_inside = false;
  assert(usb_test_receive("x", 1));
  usb_test_run_tasks();
  assert(usb_test_rx_armed());

  // Even if the new delivery runs before deferred disconnect cleanup,
  // a retained old snapshot is not overwritten until its owner resumes RX.
  resume_inside = false;
  assert(usb_test_receive("x", 1));
  usb_test_run_tasks();
  assert(retained_data[0] == 'x');
  unsigned before = delivered;
  usb_test_unplug();
  usb_test_replug();
  usb_test_dtr(true);
  assert(usb_test_receive("y", 1));
  usb_test_run_last_task();
  assert(delivered == before && retained_data[0] == 'x');
  usbd_cdc_resume_rx(&cdc);
  pause = false;
  usb_test_run_tasks();
  assert(delivered == before + 1 && retained_data[0] == 'y');
  assert(usb_test_rx_armed());

  // Without an RX callback, data is still consumed and the endpoint rearmed.
  cdc.rx_cb = NULL;
  assert(usb_test_receive("x", 1));
  usb_test_run_tasks();
  assert(usb_test_rx_armed());
  puts("PASS: CDC task-context pause/resume, retained packets and ZLP rearming");
}
