/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * STM32 TWI (I2C) support.
 *
 * Targets the "I2C v2" flavor of the STM32 I2C peripheral (F0/F3/G0/G4
 * and later). The older "I2C v1" peripheral that shipped on F1/F2/F4/L1
 * is no longer supported.
 */

#include <string.h>

#include "core/dma.h"
#include "core/hardware.h"
#include "core/hardware_types.h"
#include "core/rulos.h"

#if defined(RULOS_ARM_stm32f0)
#include "stm32f0xx_ll_i2c.h"
#include "stm32f0xx_ll_rcc.h"

#elif defined(RULOS_ARM_stm32f3)
#include "stm32f3xx_ll_bus.h"
#include "stm32f3xx_ll_i2c.h"
#include "stm32f3xx_ll_rcc.h"

#define rI2C1_CLK_ENABLE() __HAL_RCC_I2C1_CLK_ENABLE()
#define rI2C1_SDA_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define rI2C1_SCL_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define rI2C1_FORCE_RESET() __HAL_RCC_I2C1_FORCE_RESET()
#define rI2C1_RELEASE_RESET() __HAL_RCC_I2C1_RELEASE_RESET()

#ifdef RULOS_I2C_ALT_MAPPING
#define rI2C1_SCL_PIN GPIO_PIN_8
#define rI2C1_SCL_GPIO_PORT GPIOB
#define rI2C1_SDA_PIN GPIO_PIN_9
#define rI2C1_SDA_GPIO_PORT GPIOB
#else
#define rI2C1_SCL_PIN GPIO_PIN_6
#define rI2C1_SCL_GPIO_PORT GPIOB
#define rI2C1_SDA_PIN GPIO_PIN_7
#define rI2C1_SDA_GPIO_PORT GPIOB
#endif
#define rI2C1_GPIO_ALTFUNC GPIO_AF4_I2C1

#elif defined(RULOS_ARM_stm32g0)
#include "stm32g0xx_ll_bus.h"
#include "stm32g0xx_ll_i2c.h"
#include "stm32g0xx_ll_rcc.h"

#define rI2C1_CLK_ENABLE() __HAL_RCC_I2C1_CLK_ENABLE()
#define rI2C1_SDA_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define rI2C1_SCL_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define rI2C1_FORCE_RESET() __HAL_RCC_I2C1_FORCE_RESET()
#define rI2C1_RELEASE_RESET() __HAL_RCC_I2C1_RELEASE_RESET()

#define rI2C1_SCL_PIN GPIO_PIN_6
#define rI2C1_SCL_GPIO_PORT GPIOB
#define rI2C1_SDA_PIN GPIO_PIN_7
#define rI2C1_SDA_GPIO_PORT GPIOB
#define rI2C1_GPIO_ALTFUNC GPIO_AF4_I2C1

#else
#error "Your chip's definitions for I2C registers could use some help."
#include <stophere>
#endif

#define rI2C1_DMA_TX_REG_ADDR()                       \
  ((volatile void *)(uintptr_t)LL_I2C_DMA_GetRegAddr( \
      I2C1, LL_I2C_DMA_REG_DATA_TRANSMIT))
#define rI2C1_DMA_RX_REG_ADDR()                       \
  ((volatile void *)(uintptr_t)LL_I2C_DMA_GetRegAddr( \
      I2C1, LL_I2C_DMA_REG_DATA_RECEIVE))

typedef struct {
  MediaStateIfc media;
  I2C_TypeDef *handle;

  // DMA channels for TX/RX (allocated via rulos_dma in hal_twi_init).
  rulos_dma_channel_t *tx_dma_ch;
  rulos_dma_channel_t *rx_dma_ch;

  // sending
  MediaSendDoneFunc send_done_cb;
  void *send_done_cb_data;
  uint8_t send_addr;
  // send_data and send_len are stashed on the initial twi_send so
  // that ARLO retries can re-arm the TX DMA from the same buffer
  // without needing the caller to re-submit.
  const void *send_data;
  uint8_t send_len;
  uint32_t packets_sent;
  uint32_t packets_nacked;
  uint32_t send_errors;
  // When set, an earlier master-send attempt lost arbitration and is
  // waiting to be re-launched. send_done_cb stays non-null (so the
  // user can't queue another send) but the EV/ER dispatch gates off
  // retry_pending so the slave-receive path can service the winning
  // master's packet in the meantime.
  bool send_retry_pending;
  uint32_t send_arlo_retries;

  // receiving
  MediaRecvSlot *slaveRecvSlot;
} TwiState;

// Right now the RULOS HAL interface doesn't even support multiple
// TWIs; I'm trying to structure the code in such a way that it won't
// be a violent change once it does.
#define NUM_TWI 1
static TwiState g_twi[NUM_TWI];

/// util ////

static void reset_bus(TwiState *twi) {
  (void)twi;
  LL_I2C_Disable(I2C1);
  LL_I2C_Enable(I2C1);
}

/////////////////////// receiving /////////////////////////////////////////

// Warning: runs in interrupt context.
static void twi_recv_done(TwiState *twi) {
  // Real packet received!
  assert(twi->slaveRecvSlot != NULL);
  int packet_len = twi->slaveRecvSlot->capacity -
                   rulos_dma_get_remaining(twi->rx_dma_ch);

  if (packet_len > 0) {
    twi->slaveRecvSlot->packet_len = packet_len;
    twi->slaveRecvSlot->func(twi->slaveRecvSlot);
  }
}

// Warning: runs in interrupt context.
static void twi_recv_event_interrupt(TwiState *twi) {
  // TODO: make I2C1 a constant inside TwiState if we ever have a
  // second TWI interface
  if (LL_I2C_IsActiveFlag_ADDR(I2C1)) {
    // New packet has arrived. Set up a DMA transfer and ack the packet.
    rulos_dma_start(twi->rx_dma_ch, rI2C1_DMA_RX_REG_ADDR(),
                    &twi->slaveRecvSlot->data[0],
                    twi->slaveRecvSlot->capacity);
    LL_I2C_EnableDMAReq_RX(I2C1);
    LL_I2C_ClearFlag_ADDR(I2C1);
  } else if (LL_I2C_IsActiveFlag_STOP(I2C1)) {
    // Stop detected. Clear the stop flag and run the receive handler.
    LL_I2C_ClearFlag_STOP(I2C1);
    LL_I2C_DisableDMAReq_RX(I2C1);
    rulos_dma_stop(twi->rx_dma_ch);
    twi_recv_done(twi);
  }
}

// Warning: runs in interrupt context.
static void twi_recv_error_interrupt(TwiState *twi) {
  (void)twi;
  LL_I2C_DisableDMAReq_RX(I2C1);
}

// DMA TC callback: the RX channel fires TC only if the receive buffer
// fills (the normal stop path uses the I2C EV IRQ + STOP detect, not
// DMA TC). Stop accepting data.
static void twi_rx_dma_tc_callback(void *user_data) {
  (void)user_data;
  LL_I2C_DisableDMAReq_RX(I2C1);
  LL_I2C_AcknowledgeNextData(I2C1, LL_I2C_NACK);
}

/////////////////////// sending /////////////////////////////////////////

#define MASTER_ACTIVE(twi) ((twi)->send_done_cb != NULL)

// Warning: runs in interrupt context.
static void twi_send_done_upcall(TwiState *twi) {
  assert(MASTER_ACTIVE(twi));

  // NB: send_done_cb must be nulled before the callback runs, since
  // the callback is likely to want to send another packet. But,
  // schedule_now doesn't run the callback until the scheduler runs
  // again, so this works.
  schedule_now((ActivationFuncPtr)twi->send_done_cb, twi->send_done_cb_data);
  twi->send_done_cb = NULL;
  twi->send_done_cb_data = NULL;
}

// Warning: runs in interrupt context.
static void twi_send_event_interrupt(TwiState *twi) {
  // Transmit-complete: successful transmission.
  if (LL_I2C_IsActiveFlag_STOP(I2C1)) {
    LL_I2C_ClearFlag_STOP(I2C1);

    // Check to see if we got a NACK. Doesn't really do anything but
    // statistics collection.
    if (LL_I2C_IsActiveFlag_NACK(I2C1)) {
      LL_I2C_ClearFlag_NACK(I2C1);
      twi->packets_nacked++;
    } else {
      twi->packets_sent++;
    }
    twi_send_done_upcall(twi);
  }
}

// Warning: runs in interrupt context.
static void twi_send_error_interrupt(TwiState *twi) {
  // AUTOEND means the hardware has already generated STOP; we just
  // need to upcall the user.
  twi->send_errors++;
  twi_send_done_upcall(twi);
}

// DMA TC callback for the I2C TX channel. Disables the peripheral's
// DMA request so the autostop sequence can complete.
static void twi_tx_dma_tc_callback(void *user_data) {
  (void)user_data;
  LL_I2C_DisableDMAReq_TX(I2C1);
}

// Arm the TX DMA channel and kick off a master write using whatever
// is currently stashed in twi->send_data / send_len / send_addr. Used
// both for fresh sends (from twi_send) and for ARLO retries (from the
// slave-receive STOP handler and the twi_arlo_retry_poll activation).
//
// Caller is responsible for bus state: on fresh sends, twi_send waits
// for BUSY to clear before calling this; on retries, the caller has
// either just observed STOP on a slave-receive or confirmed BUSY is
// clear via poll. Even if the bus goes busy between the check and
// here, HandleTransfer's START-bit pattern causes the hardware to
// wait for bus idle before actually issuing START, so we either win
// the next arbitration or ARLO and retry again.
static void twi_launch_send(TwiState *twi) {
  rulos_dma_start(twi->tx_dma_ch, rI2C1_DMA_TX_REG_ADDR(),
                  (void *)twi->send_data, twi->send_len);
  LL_I2C_EnableDMAReq_TX(I2C1);
  LL_I2C_AcknowledgeNextData(I2C1, LL_I2C_ACK);
  LL_I2C_HandleTransfer(I2C1, twi->send_addr, LL_I2C_ADDRSLAVE_7BIT,
                        twi->send_len, LL_I2C_MODE_AUTOEND,
                        LL_I2C_GENERATE_START_WRITE);
}

// Scheduled polling activation for the "pure ARLO" retry path: we
// lost arbitration but weren't addressed ourselves, so no EV IRQ
// (ADDR/STOP) will ever arrive to tell us the bus is idle again.
// Poll BUSY periodically and re-launch when clear. The ADDR+ARLO
// path also schedules this as a belt-and-suspenders fallback in
// case the slave-receive STOP path doesn't fire for some reason;
// whichever side wins the race just clears send_retry_pending and
// the other one no-ops.
static void twi_arlo_retry_poll(void *data) {
  TwiState *twi = (TwiState *)data;

  rulos_irq_state_t old_interrupts = hal_start_atomic();
  if (!twi->send_retry_pending) {
    // The EV STOP path (or a previous poll tick) already re-launched
    // the send -- nothing to do.
    hal_end_atomic(old_interrupts);
    return;
  }
  if (LL_I2C_IsActiveFlag_BUSY(I2C1)) {
    // Bus is still in use -- most likely by the master that just beat
    // us. Come back in a millisecond.
    hal_end_atomic(old_interrupts);
    schedule_us(1000, twi_arlo_retry_poll, twi);
    return;
  }

  twi->send_retry_pending = false;
  twi_launch_send(twi);
  hal_end_atomic(old_interrupts);
}

static void twi_send(MediaStateIfc *media, Addr dest_addr, const void *data,
                     uint8_t len, MediaSendDoneFunc send_done_cb,
                     void *send_done_cb_data) {
#ifdef TIMING_DEBUG_PIN
  gpio_set(TIMING_DEBUG_PIN);
#endif

  // If we ever support multiple TWI interfaces, add a lookup based on
  // id here.
  TwiState *twi = &g_twi[0];

  // !MASTER_ACTIVE also covers !send_retry_pending: send_retry_pending
  // is only ever set when send_done_cb is non-null, and is cleared by
  // the retry launch path before send_done_cb itself is nulled.
  assert(!MASTER_ACTIVE(twi));

  // Wait for the current transaction to be complete, if any. Then,
  // atomically enable master mode. Bound the wait so a wedged BUSY
  // flag (e.g. a stuck SCL/SDA) can't hang the CPU indefinitely here;
  // 100 ms is well above any legitimate multimaster collision or
  // slave-receive window, so hitting it means the peripheral needs a
  // reset. reset_bus just toggles PE, which preserves CR1 and the
  // DMA channel config, so the launch below will proceed cleanly on
  // the recovered peripheral.
  const Time busy_deadline = precise_clock_time_us() + 100000;
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  while (LL_I2C_IsActiveFlag_BUSY(I2C1)) {
    hal_end_atomic(old_interrupts);
    __WFI();
    old_interrupts = hal_start_atomic();
    if ((int32_t)(precise_clock_time_us() - busy_deadline) >= 0) {
      reset_bus(twi);
      break;
    }
  }

  twi->send_done_cb = send_done_cb;
  twi->send_done_cb_data = send_done_cb_data;
  twi->send_addr = dest_addr << 1;  // add "write" bit
  twi->send_data = data;
  twi->send_len = len;
  twi_launch_send(twi);
  hal_end_atomic(old_interrupts);

#ifdef TIMING_DEBUG_PIN
  gpio_clr(TIMING_DEBUG_PIN);
#endif
}

//////////////////////////////////////////////////////////////////////////

MediaStateIfc *hal_twi_init(uint32_t speed_khz, Addr local_addr,
                            MediaRecvSlot *const slaveRecvSlot) {
  // Someday, if we make the HAL interface support multiple TWI
  // interfaces, this line should be changed from [0] to [index].
  TwiState *twi = &g_twi[0];
  memset(twi, 0, sizeof(TwiState));
  twi->handle = I2C1;
  twi->media.send = &twi_send;
  twi->slaveRecvSlot = slaveRecvSlot;

  // Enable clocks (DMA clock is already enabled by core/stm32-hardware.c).
  rI2C1_SCL_GPIO_CLK_ENABLE();
  rI2C1_SDA_GPIO_CLK_ENABLE();
  rI2C1_CLK_ENABLE();

  reset_bus(twi);

  // Configure GPIO
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Pin = rI2C1_SCL_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
#ifdef rI2C1_GPIO_ALTFUNC
  GPIO_InitStruct.Alternate = rI2C1_GPIO_ALTFUNC;
#endif
  HAL_GPIO_Init(rI2C1_SCL_GPIO_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = rI2C1_SDA_PIN;
  HAL_GPIO_Init(rI2C1_SDA_GPIO_PORT, &GPIO_InitStruct);

  // Allocate DMA TX channel
  rulos_dma_config_t tx_cfg = {
      .request = RULOS_DMA_REQ_I2C1_TX,
      .direction = RULOS_DMA_DIR_MEM_TO_PERIPH,
      .mode = RULOS_DMA_MODE_NORMAL,
      .periph_width = RULOS_DMA_WIDTH_BYTE,
      .mem_width = RULOS_DMA_WIDTH_BYTE,
      .periph_increment = false,
      .mem_increment = true,
      .priority = RULOS_DMA_PRIORITY_LOW,
      .tc_callback = twi_tx_dma_tc_callback,
      .user_data = twi,
  };
  twi->tx_dma_ch = rulos_dma_alloc(&tx_cfg);
  assert(twi->tx_dma_ch != NULL);

  // Allocate DMA RX channel
  rulos_dma_config_t rx_cfg = {
      .request = RULOS_DMA_REQ_I2C1_RX,
      .direction = RULOS_DMA_DIR_PERIPH_TO_MEM,
      .mode = RULOS_DMA_MODE_NORMAL,
      .periph_width = RULOS_DMA_WIDTH_BYTE,
      .mem_width = RULOS_DMA_WIDTH_BYTE,
      .periph_increment = false,
      .mem_increment = true,
      .priority = RULOS_DMA_PRIORITY_HIGH,
      .tc_callback = twi_rx_dma_tc_callback,
      .user_data = twi,
  };
  twi->rx_dma_ch = rulos_dma_alloc(&rx_cfg);
  assert(twi->rx_dma_ch != NULL);

  // Set up I2C
  LL_I2C_Disable(I2C1);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1);
  LL_RCC_SetI2CClockSource(LL_RCC_I2C1_CLKSOURCE_SYSCLK);
  // I can't figure out from the datasheet how to adjust these numbers
  // from first principles to make them correct. I'm kinda unhappy
  // with the STM32 I2C peripheral's timing setup; the datasheet is
  // extremely hard to understand and there are no helper functions in
  // the code. They say "just ask our configuration tool", which you
  // can't translate to runtime code. The constants below are manually
  // adjusted for 100khz, 200khz and 400khz when the CPU is at 64
  // Mhz. Sorry.
  uint32_t timingr;
  if (speed_khz == 400) {
    timingr = __LL_I2C_CONVERT_TIMINGS(0,   // Prescaler
                                       13,  // SCLDEL, data setup time
                                       13,  // SDADEL, data hold time
                                       44,  // SCLH, clock high period
                                       74   // SCLL, clock low period
    );
  } else if (speed_khz == 200) {
    timingr = __LL_I2C_CONVERT_TIMINGS(1,   // Prescaler
                                       13,  // SCLDEL, data setup time
                                       13,  // SDADEL, data hold time
                                       54,  // SCLH, clock high period
                                       84   // SCLL, clock low period
    );
  } else if (speed_khz == 100) {
    timingr = __LL_I2C_CONVERT_TIMINGS(1,    // Prescaler
                                       13,   // SCLDEL, data setup time
                                       13,   // SDADEL, data hold time
                                       117,  // SCLH, clock high period
                                       181   // SCLL, clock low period
    );
  } else {
    __builtin_trap();
  }
  LL_I2C_SetTiming(I2C1, timingr);

  LL_I2C_SetOwnAddress1(I2C1, local_addr << 1, LL_I2C_OWNADDRESS1_7BIT);
  LL_I2C_EnableOwnAddress1(I2C1);
  LL_I2C_EnableClockStretching(I2C1);
  LL_I2C_SetMode(I2C1, LL_I2C_MODE_I2C);

  // Turn on interrupts
  NVIC_SetPriority(I2C1_ER_IRQn, 0x2);
  NVIC_EnableIRQ(I2C1_ER_IRQn);
  NVIC_SetPriority(I2C1_EV_IRQn, 0x2);
  NVIC_EnableIRQ(I2C1_EV_IRQn);

  LL_I2C_EnableIT_STOP(I2C1);
  LL_I2C_EnableIT_ADDR(I2C1);
  LL_I2C_EnableIT_ERR(I2C1);

  // Enable!
  LL_I2C_Enable(I2C1);

  // Turn on acks for incoming receives
  LL_I2C_AcknowledgeNextData(I2C1, LL_I2C_ACK);

#ifdef TIMING_DEBUG_PIN
  gpio_make_output(TIMING_DEBUG_PIN);
#endif

  return &twi->media;
}

//// interrupt handler trampolines

void I2C1_EV_IRQHandler(void) {
  TwiState *twi = &g_twi[0];

  // Multimaster race handling: ADDR can fire while MASTER_ACTIVE is
  // still true if we just lost arbitration to a master that happens
  // to be addressing us as slave. The ER IRQ for the accompanying
  // ARLO would normally clean up master state and let the dispatch
  // below route ADDR to the slave-receive path, but EV and ER are
  // both configured at NVIC priority 0x2, and EV has the lower IRQ
  // number (31 vs 32 on F303), so ER cannot preempt us here. If we
  // return without touching ADDR, NVIC tail-chains back into this
  // handler immediately and we live-lock while ER is starved. Mark
  // the send as retry-pending and fall through to the slave-receive
  // path; the retry will be kicked once the winning master's
  // transaction completes.
  if (LL_I2C_IsActiveFlag_ADDR(I2C1) && MASTER_ACTIVE(twi) &&
      !twi->send_retry_pending) {
    // Clear ARLO ourselves if it's set; otherwise, when ER does
    // finally run (after we clear ADDR), it would find no error
    // flags set and would misdispatch to twi_recv_error_interrupt,
    // which disables the RX DMA we're about to enable. (The ER
    // handler is also tightened below to skip dispatch when no flag
    // was actually handled, as a belt-and-suspenders.)
    if (LL_I2C_IsActiveFlag_ARLO(I2C1)) {
      LL_I2C_ClearFlag_ARLO(I2C1);
    }
    twi->send_retry_pending = true;
    twi->send_arlo_retries++;
    // Primary retry kick: after the slave-receive below completes
    // with STOP, the tail of this handler re-launches the send. The
    // poll activation is a belt-and-suspenders fallback in case that
    // STOP path doesn't fire.
    schedule_us(1000, twi_arlo_retry_poll, twi);
    // send_done_cb is intentionally NOT nulled: we want the retry to
    // be transparent to the caller. The dispatch below gates the
    // master-send path on !send_retry_pending so the slave-receive
    // path gets to run.
  }

  if (MASTER_ACTIVE(twi) && !twi->send_retry_pending) {
    twi_send_event_interrupt(twi);
  } else {
    twi_recv_event_interrupt(twi);
  }

  // If we're holding a retry from an ARLO'd send and the slave-receive
  // path above just drained the bus (STOP detected, BUSY cleared),
  // re-launch immediately. This covers the common ADDR+ARLO case
  // without waiting for twi_arlo_retry_poll to fire.
  if (twi->send_retry_pending && !LL_I2C_IsActiveFlag_BUSY(I2C1)) {
    twi->send_retry_pending = false;
    twi_launch_send(twi);
  }
}

void I2C1_ER_IRQHandler(void) {
  TwiState *twi = &g_twi[0];

  // In the future, if the send-done callback wants to differentiate
  // between different kinds of error, it can be recorded here and
  // passed into twi_send_done.
  bool handled_any = false;
  if (LL_I2C_IsActiveFlag_BERR(I2C1)) {
    // Bus error. Reset the I2C interface in hopes of recovering.
    LL_I2C_ClearFlag_BERR(I2C1);
    reset_bus(twi);
    handled_any = true;
  }
  if (LL_I2C_IsActiveFlag_ARLO(I2C1)) {
    // Arbitration lost, meaning we couldn't acquire the bus.
    LL_I2C_ClearFlag_ARLO(I2C1);
    handled_any = true;
    // Pure ARLO (not accompanied by ADDR -- the ADDR+ARLO case is
    // handled by I2C1_EV_IRQHandler before ER can run). We lost to
    // a master that isn't addressing us, so no slave-receive STOP
    // will kick us when the bus clears. Schedule a poll task to
    // re-launch the send once BUSY drops.
    if (MASTER_ACTIVE(twi) && !twi->send_retry_pending) {
      twi->send_retry_pending = true;
      twi->send_arlo_retries++;
      schedule_us(1000, twi_arlo_retry_poll, twi);
    }
  }
  if (LL_I2C_IsActiveFlag_OVR(I2C1)) {
    LL_I2C_ClearFlag_OVR(I2C1);
    handled_any = true;
  }

  // If no error flag was set we were tail-chained into with nothing to
  // do -- most likely because I2C1_EV_IRQHandler already cleared an
  // ARLO inline as part of the ADDR-while-master multimaster race
  // handling. Dispatching to twi_recv_error_interrupt in that case
  // would disable the RX DMA request that the EV handler just set up.
  if (!handled_any) {
    return;
  }

  // If a retry is pending, skip the send_error_interrupt path --
  // it would null send_done_cb and upcall the user, dropping the
  // packet we're trying to transparently retry.
  if (MASTER_ACTIVE(twi) && !twi->send_retry_pending) {
    twi_send_error_interrupt(twi);
  } else {
    twi_recv_error_interrupt(twi);
  }
}
