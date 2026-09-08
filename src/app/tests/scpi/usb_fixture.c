#include "usb_fixture.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/clock.h"
#include "core/hal.h"

static struct {
  ActivationFuncPtr func;
  void *data;
} tasks[128];
static unsigned task_head, task_tail;
static USBD_CDC_ItfTypeDef *callbacks;
static uint8_t *rx_buffer;
static uint8_t *tx_buffer;
static uint32_t tx_size;
static bool connected, armed, transmitting;
static unsigned tx_count, arm_count;
static char last_tx[2048];
static rulos_irq_state_t irq_mask;

rulos_irq_state_t hal_start_atomic(void) {
  rulos_irq_state_t previous = irq_mask;
  irq_mask = 1;
  return previous;
}

void hal_end_atomic(rulos_irq_state_t previous) {
  irq_mask = previous;
}

void log_assert(const char *file, unsigned long line) {
  fprintf(stderr, "Assertion failed: %s:%lu\n", file, line);
  abort();
}

void schedule_now(ActivationFuncPtr func, void *data) {
  assert(task_head - task_tail < sizeof(tasks) / sizeof(tasks[0]));
  tasks[task_head % 128].func = func;
  tasks[task_head++ % 128].data = data;
}

void schedule_us(Time offset, ActivationFuncPtr func, void *data) {
  assert(!"DFU is not part of this fixture");
}

void usb_test_run_tasks(void) {
  unsigned budget = 10000;
  while (task_tail != task_head) {
    assert(budget--);
    ActivationFuncPtr func = tasks[task_tail % 128].func;
    void *data = tasks[task_tail++ % 128].data;
    func(data);
  }
}

void usb_test_run_last_task(void) {
  assert(task_tail != task_head);
  task_head--;
  tasks[task_head % 128].func(tasks[task_head % 128].data);
}

void rulos_dfu_enter_bootloader(void) {
  assert(!"unexpected DFU request");
}

void usbd_cdc_get_serial(char *out) {
  strcpy(out, "000000000000000000000001");
}

static uint8_t *descriptor(uint16_t *len) {
  static uint8_t bytes[USB_CDC_CONFIG_DESC_SIZ] = {9, USB_DESC_TYPE_CONFIGURATION, 0, 0, 2};
  *len = sizeof(bytes);
  return bytes;
}

USBD_DescriptorsTypeDef CDC_Desc;
USBD_ClassTypeDef USBD_CDC = {
    .GetFSConfigDescriptor = descriptor,
    .GetHSConfigDescriptor = descriptor,
    .GetOtherSpeedConfigDescriptor = descriptor,
};

USBD_StatusTypeDef USBD_Init(USBD_HandleTypeDef *device, USBD_DescriptorsTypeDef *desc,
                             uint8_t id) {
  return USBD_OK;
}
USBD_StatusTypeDef USBD_RegisterClass(USBD_HandleTypeDef *device, USBD_ClassTypeDef *class) {
  return USBD_OK;
}
uint8_t USBD_CDC_RegisterInterface(USBD_HandleTypeDef *device, USBD_CDC_ItfTypeDef *interface) {
  callbacks = interface;
  return USBD_OK;
}
USBD_StatusTypeDef USBD_Start(USBD_HandleTypeDef *device) {
  usb_test_replug();
  return USBD_OK;
}
uint8_t USBD_CDC_SetRxBuffer(USBD_HandleTypeDef *device, uint8_t *buffer) {
  rx_buffer = buffer;
  return USBD_OK;
}
uint8_t USBD_CDC_ReceivePacket(USBD_HandleTypeDef *device) {
  if (!connected) {
    return USBD_FAIL;
  }
  assert(!armed);
  armed = true;
  arm_count++;
  return USBD_OK;
}
uint8_t USBD_CDC_SetTxBuffer(USBD_HandleTypeDef *device, uint8_t *buffer, uint32_t size) {
  assert(!transmitting);
  tx_buffer = buffer;
  tx_size = size;
  return USBD_OK;
}
uint8_t USBD_CDC_TransmitPacket(USBD_HandleTypeDef *device) {
  assert(connected && !transmitting);
  transmitting = true;
  return USBD_OK;
}

void usb_test_dtr(bool open) {
  USBD_SetupReqTypedef req = {.wValue = open};
  assert(callbacks->Control(CDC_SET_CONTROL_LINE_STATE, (uint8_t *)&req, 0) == USBD_OK);
}
void usb_test_unplug(void) {
  assert(!irq_mask);
  connected = false;
  armed = false;
  transmitting = false;
  assert(callbacks->DeInit() == USBD_OK);
}
void usb_test_replug(void) {
  connected = true;
  assert(callbacks->Init() == USBD_OK);
  // USBD_CDC_Init unconditionally prepares OUT after the interface Init
  // callback. A duplicate arm here means the driver rearmed inside Init.
  assert(!armed);
  armed = true;
  arm_count++;
}
bool usb_test_receive(const char *data, uint32_t len) {
  assert(!irq_mask);
  assert(len <= 64);
  if (!armed) {
    return false;
  }
  armed = false;
  memcpy(rx_buffer, data, len);
  assert(callbacks->Receive(rx_buffer, &len) == USBD_OK);
  return true;
}
bool usb_test_complete(void) {
  if (!transmitting) {
    return false;
  }
  assert(tx_size < sizeof(last_tx));
  // Capture at completion, not submission: mutation during flight is a bug.
  memcpy(last_tx, tx_buffer, tx_size);
  last_tx[tx_size] = 0;
  tx_count++;
  transmitting = false;
  assert(callbacks->TransmitCplt(tx_buffer, &tx_size, CDC_IN_EP) == USBD_OK);
  return true;
}
const char *usb_test_last_tx(void) {
  return last_tx;
}
unsigned usb_test_tx_count(void) {
  return tx_count;
}
unsigned usb_test_arm_count(void) {
  return arm_count;
}
bool usb_test_rx_armed(void) {
  return armed;
}
