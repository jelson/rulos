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

#include "core/hal.h"
#include "core/hardware.h"

#undef TRUE
#undef FALSE

#include "chip/arm/lpc_chip_11cxx_lib/i2c_11xx.h"

typedef struct {
  MediaStateIfc media;
} TwiState;

static TwiState g_twi;

static void handle_interrupt(I2C_ID_T id) {
  if (Chip_I2C_IsMasterActive(id)) {
    Chip_I2C_MasterStateHandler(id);
  } else {
    Chip_I2C_SlaveStateHandler(id);
  }
}

void I2C_Handler(void) { handle_interrupt(I2C0); }

static void twi_send(MediaStateIfc *media, Addr dest_addr,
                     const unsigned char *data, uint8_t len,
                     MediaSendDoneFunc sendDoneCB, void *sendDoneCBData) {
  Chip_I2C_MasterSend(I2C0, dest_addr, data, len);
  sendDoneCB(sendDoneCBData);
}

MediaStateIfc *hal_twi_init(uint32_t speed_khz, Addr local_addr,
                            MediaRecvSlot *const slaveRecvSlot) {
  Chip_SYSCTL_PeriphReset(RESET_I2C0);
  gpio_iocon(GPIO0_04, IOCON_FUNC1 | IOCON_FASTI2C_EN);
  gpio_iocon(GPIO0_05, IOCON_FUNC1 | IOCON_FASTI2C_EN);
  Chip_I2C_Init(I2C0);
  Chip_I2C_SetClockRate(I2C0, speed_khz);
  Chip_I2C_SetMasterEventHandler(I2C0, Chip_I2C_EventHandler);
  NVIC_EnableIRQ(I2C0_IRQn);

  memset(&g_twi, 0, sizeof(g_twi));
  g_twi.media.send = &twi_send;
  return &g_twi.media;
}
