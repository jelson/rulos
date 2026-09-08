#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "periph/usb_cdc/usb_cdc.h"

void usb_test_run_tasks(void);
void usb_test_run_last_task(void);
void usb_test_dtr(bool open);
void usb_test_unplug(void);
void usb_test_replug(void);
bool usb_test_receive(const char *data, uint32_t len);
bool usb_test_complete(void);
const char *usb_test_last_tx(void);
unsigned usb_test_tx_count(void);
unsigned usb_test_arm_count(void);
bool usb_test_rx_armed(void);
