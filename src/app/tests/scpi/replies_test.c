#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "periph/scpi/scpi.h"
#include "usb_fixture.h"

static unsigned query_count;
static bool stream_enabled;

static bool on_line(const char *line) {
  if (strcmp(line, "Q?") == 0) {
    char response[32];
    snprintf(response, sizeof(response), "VALUE=%u", ++query_count);
    assert(scpi_print(response));
    return true;
  }
  return false;
}

static void on_tx_complete(void) {
  usbd_cdc_state_t *cdc = scpi_usb_cdc_handle();
  if (stream_enabled && usbd_cdc_tx_ready(cdc)) {
    assert(usbd_cdc_print(cdc, "#STREAM\n") == 0);
  }
}

static void receive(const char *s) {
  assert(usb_test_receive(s, strlen(s)));
  usb_test_run_tasks();
}

static void expect_reply(const char *expected) {
  assert(usb_test_complete());
  assert(strcmp(usb_test_last_tx(), expected) == 0);
  usb_test_run_tasks();
}

static void expect_value(unsigned value) {
  char expected[32];
  snprintf(expected, sizeof(expected), "VALUE=%u\n", value);
  expect_reply(expected);
}

static void test_bursts(void) {
  scpi_set_error("-100,\"original error\"");
  receive("SYST:ERR?\nSYST:ERR?\nQ?\n");
  expect_reply("-100,\"original error\"\n");
  expect_reply("0,\"No error\"\n");
  expect_value(1);
  assert(!usb_test_complete());

  // Sixteen replies from one full OUT packet must not overflow a reply
  // queue, rearm prematurely, or execute the next command before completion.
  const char *packet =
      " Q?\n Q?\n Q?\n Q?\n Q?\n Q?\n Q?\n Q?\n Q?\n Q?\n Q?\n Q?\n Q?\n Q?\n Q?\n Q?\n";
  assert(strlen(packet) == 64);
  unsigned first = query_count + 1;
  receive(packet);
  for (unsigned i = 0; i < 16; i++) {
    assert(query_count == first + i);
    assert(!usb_test_receive("Q?\n", 3));
    expect_value(first + i);
  }
  assert(usb_test_rx_armed());

  // Exercise many physical packets while streaming continuously. Each
  // pending command and each streaming producer gets a TX turn.
  stream_enabled = true;
  on_tx_complete();
  for (unsigned batch = 0; batch < 32; batch++) {
    first = query_count + 1;
    receive(packet);
    for (unsigned i = 0; i < 16; i++) {
      expect_reply("#STREAM\n");
      expect_value(first + i);
    }
  }
  stream_enabled = false;
  expect_reply("#STREAM\n");
}

static void test_fragments(void) {
  receive("Q");
  assert(!usb_test_complete());
  receive("?\r");
  expect_value(query_count);
  unsigned next = query_count + 1;
  receive("\nQ?\r\n");
  expect_value(next);
  assert(!usb_test_complete());

  receive("SYST:ER");
  usb_test_dtr(false);
  usb_test_run_tasks();
  usb_test_dtr(true);
  usb_test_run_tasks();
  receive("Q?\n");
  expect_value(next + 1);
}

static void test_error_snapshots(void) {
  assert(usbd_cdc_print(scpi_usb_cdc_handle(), "#BUSY\n") == 0);
  scpi_set_error("-1,\"old\"");
  receive("SYST:ERR?\nSYST:ERR?\n");
  assert(!scpi_print("must not overwrite the pending error"));
  scpi_set_error("-2,\"new\"");
  expect_reply("#BUSY\n");
  expect_reply("-1,\"old\"\n");
  expect_reply("-2,\"new\"\n");
  receive("SYST:ERR?\n");
  expect_reply("0,\"No error\"\n");
}

static void test_disconnect(void) {
  assert(usbd_cdc_print(scpi_usb_cdc_handle(), "#BUSY\n") == 0);
  scpi_set_error("-3,\"unsent\"");
  receive("SYST:ERR?\nQ?\n");
  unsigned before = query_count;
  usb_test_dtr(false);
  usb_test_run_tasks();
  expect_reply("#BUSY\n");
  usb_test_dtr(true);
  usb_test_run_tasks();
  assert(!usb_test_complete());
  assert(query_count == before);
  receive("SYST:ERR?\n");
  expect_reply("-3,\"unsent\"\n");

  // Closing DTR leaves submitted TX storage owned until completion.
  receive("Q?\n");
  usb_test_dtr(false);
  usb_test_run_tasks();
  assert(!scpi_print("must not modify an in-flight reply"));
  expect_value(before + 1);
  usb_test_dtr(true);
  usb_test_run_tasks();

  scpi_set_error("-4,\"cancelled\"");
  receive("SYST:ERR?\n");
  usb_test_unplug();
  usb_test_run_tasks();
  usb_test_replug();
  usb_test_dtr(true);
  usb_test_run_tasks();
  receive("SYST:ERR?\n");
  expect_reply("-4,\"cancelled\"\n");

  // A completion queued before a bus reset must not release a buffer or
  // consume an error after deinitialization cancelled its callback.
  scpi_set_error("-5,\"reset before callback\"");
  receive("SYST:ERR?\n");
  assert(usb_test_complete());
  usb_test_unplug();
  usb_test_run_tasks();
  usb_test_replug();
  usb_test_dtr(true);
  usb_test_run_tasks();
  receive("SYST:ERR?\n");
  expect_reply("-5,\"reset before callback\"\n");

  // DTR close followed by physical removal must cancel the retained reply
  // even though CDC's ready flag was already false at deinitialization.
  scpi_set_error("-6,\"closed then removed\"");
  receive("SYST:ERR?\n");
  usb_test_dtr(false);
  usb_test_run_tasks();
  usb_test_unplug();
  usb_test_replug();
  usb_test_dtr(true);
  usb_test_run_tasks();
  receive("SYST:ERR?\n");
  expect_reply("-6,\"closed then removed\"\n");

  // Do not drain the scheduler during unplug/replug. An old RX delivery
  // must not consume the new session's packet before its disconnect reset.
  assert(usb_test_receive("SYST:ERR?\n", 10));
  usb_test_unplug();
  usb_test_replug();
  usb_test_dtr(true);
  assert(usb_test_receive("Q?\n", 3));
  before = query_count;
  usb_test_run_tasks();
  expect_value(before + 1);

  // Likewise, a stale completion must not clear a new session's error or
  // release its reply buffer, even when reconnect happens before any task.
  scpi_set_error("-7,\"rapid reconnect\"");
  receive("SYST:ERR?\n");
  assert(usb_test_complete());
  usb_test_unplug();
  usb_test_replug();
  usb_test_dtr(true);
  usb_test_run_tasks();
  receive("SYST:ERR?\nQ?\n");
  expect_reply("-7,\"rapid reconnect\"\n");
  expect_value(before + 2);

  // An application task already queued before unplug can start a new
  // stream transfer before deferred disconnect cleanup. Its busy flag does
  // not mean that the canceled SCPI reply still owns the endpoint.
  scpi_set_error("-8,\"canceled before new stream\"");
  receive("SYST:ERR?\n");
  usb_test_unplug();
  usb_test_replug();
  usb_test_dtr(true);
  assert(usbd_cdc_print(scpi_usb_cdc_handle(), "#NEWSTREAM\n") == 0);
  assert(usb_test_complete());
  assert(strcmp(usb_test_last_tx(), "#NEWSTREAM\n") == 0);
  usb_test_run_last_task();  // Complete new TX before queued disconnect/connect.
  usb_test_run_tasks();
  receive("SYST:ERR?\n");
  expect_reply("-8,\"canceled before new stream\"\n");

  // Reordered connect cannot revive an old reply that had not reached USB.
  assert(usbd_cdc_print(scpi_usb_cdc_handle(), "#BUSY\n") == 0);
  scpi_set_error("-9,\"abandoned before connect\"");
  receive("SYST:ERR?\n");
  usb_test_unplug();
  usb_test_replug();
  usb_test_dtr(true);
  usb_test_run_last_task();  // Connect before disconnect.
  assert(!usb_test_complete());
  receive("SYST:ERR?\n");
  expect_reply("-9,\"abandoned before connect\"\n");

  // A late original disconnect task must not discard a new partial command
  // whose RX callback already drained old-session cleanup.
  receive("SYST:ER");
  usb_test_unplug();
  usb_test_replug();
  usb_test_dtr(true);
  assert(usb_test_receive("Q?", 2));
  usb_test_run_last_task();  // New RX before disconnect/connect.
  usb_test_run_tasks();
  before = query_count;
  receive("\n");
  expect_value(before + 1);

  // A packet received but not delivered before DTR close belongs to the
  // old session, even if close/open both precede its deferred RX callback.
  before = query_count;
  assert(usb_test_receive("Q?\n", 3));
  usb_test_dtr(false);
  usb_test_dtr(true);
  usb_test_run_tasks();
  assert(query_count == before && !usb_test_complete());
  receive("Q?\n");
  expect_value(before + 1);

  // Commands delivered while closed are ignored, and a closed-session
  // packet still awaiting delivery when DTR rises is canceled as well.
  usb_test_dtr(false);
  usb_test_run_tasks();
  receive("Q?\n");
  assert(query_count == before + 1 && !usb_test_complete());
  assert(usb_test_receive("Q?\n", 3));
  usb_test_dtr(true);
  usb_test_run_tasks();
  assert(query_count == before + 1 && !usb_test_complete());
  receive("Q?\n");
  expect_value(before + 2);
}

int main(void) {
  scpi_config_t config = {
      .on_line = on_line,
      .on_usb_tx_complete = on_tx_complete,
  };
  scpi_init(&config);
  assert(scpi_print("queued before open"));
  assert(!usb_test_complete());
  usb_test_dtr(true);
  usb_test_run_tasks();
  expect_reply("queued before open\n");
  test_bursts();
  test_fragments();
  test_error_snapshots();
  test_disconnect();
  puts("PASS: SCPI response ordering, backpressure, snapshots, streaming and reconnect");
}
