/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

/*
 * ARM TWI support
 */

#include <string.h>

#include "core/hardware.h"
#include "core/rulos.h"

#undef TRUE
#undef FALSE

#include "chip/arm/lpc_chip_11cxx_lib/i2c_11xx.h"

#define TIMING_DEBUG_PIN GPIO0_08

typedef struct {
  MediaStateIfc media;
  MediaSendDoneFunc send_done_cb;
  void *send_done_cb_data;
  I2C_XFER_T mXfer;
} TwiState;

static TwiState g_twi;

#define MASTER_ACTIVE(twi) ((twi)->send_done_cb != NULL)

static void handle_interrupt(I2C_ID_T id) {
  if (Chip_I2C_IsMasterActive(id)) {
    Chip_I2C_MasterStateHandler(id);
  } else {
    Chip_I2C_SlaveStateHandler(id);
  }
}

void I2C_IRQHandler(void) { handle_interrupt(I2C0); }

// Unlike the "state handler", which is called to drive the I2C state
// machine byte-by-byte, the "event handler" gets notifications of
// higher level events, like "send done".
//
// Warning: runs in interrupt context.
static void master_event_handler(I2C_ID_T id, I2C_EVENT_T event) {
  // If we ever support multiple TWI interfaces, add a lookup based on
  // id here.
  TwiState *twi = &g_twi;

  if (event == I2C_EVENT_DONE) {
    assert(MASTER_ACTIVE(twi));

    // NB: send_done_cb must be nulled before the callback runs, since
    // the callback is likely to want to send another packet. But,
    // schedule_now doesn't run the callback until the scheduler runs
    // again, so this works.
    schedule_now((ActivationFuncPtr)twi->send_done_cb, twi->send_done_cb_data);
    twi->send_done_cb = NULL;
    twi->send_done_cb_data = NULL;
  }
}

static void twi_send(MediaStateIfc *media, Addr dest_addr,
                     const unsigned char *data, uint8_t len,
                     MediaSendDoneFunc send_done_cb, void *send_done_cb_data) {
#ifdef TIMING_DEBUG_PIN
  gpio_set(TIMING_DEBUG_PIN);
#endif

  // If we ever support multiple TWI interfaces, add a lookup based on
  // id here.
  TwiState *twi = &g_twi;

  assert(!MASTER_ACTIVE(twi));
  assert(send_done_cb != NULL);

  twi->send_done_cb = send_done_cb;
  twi->send_done_cb_data = send_done_cb_data;
  twi->mXfer.slaveAddr = dest_addr;
  twi->mXfer.txBuff = data;
  twi->mXfer.txSz = len;
  Chip_I2C_MasterSend_Nonblocking(I2C0, &twi->mXfer);
#ifdef TIMING_DEBUG_PIN
  gpio_clr(TIMING_DEBUG_PIN);
#endif
}

MediaStateIfc *hal_twi_init(uint32_t speed_khz, Addr local_addr,
                            MediaRecvSlot *const slaveRecvSlot) {
#ifdef TIMING_DEBUG_PIN
  gpio_make_output(TIMING_DEBUG_PIN);
#endif

  Chip_SYSCTL_PeriphReset(RESET_I2C0);
  gpio_iocon(GPIO0_04, IOCON_FUNC1 | IOCON_FASTI2C_EN);
  gpio_iocon(GPIO0_05, IOCON_FUNC1 | IOCON_FASTI2C_EN);
  Chip_I2C_Init(I2C0);
  Chip_I2C_SetClockRate(I2C0, speed_khz * 1000);
  Chip_I2C_SetMasterEventHandler(I2C0, master_event_handler);
  NVIC_EnableIRQ(I2C0_IRQn);
  LPC_I2C->CONSET = I2C_CON_I2EN;

  memset(&g_twi, 0, sizeof(g_twi));
  g_twi.media.send = &twi_send;
  return &g_twi.media;
}
