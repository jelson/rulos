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
 * STM32 TWI support
 */

#include <string.h>

#include "core/dma.h"
#include "core/hardware.h"
#include "core/hardware_types.h"
#include "core/rulos.h"

#if defined(RULOS_ARM_stm32f0)
#define RULOS_I2C_V2
#include "stm32f0xx_ll_i2c.h"
#include "stm32f0xx_ll_rcc.h"

#elif defined(RULOS_ARM_stm32f1)
#define RULOS_I2C_V1
#include "stm32f1xx_ll_i2c.h"
#include "stm32f1xx_ll_rcc.h"

#define rI2C1_CLK_ENABLE() __HAL_RCC_I2C1_CLK_ENABLE()
#define rI2C1_SDA_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define rI2C1_SCL_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define rI2C1_FORCE_RESET() __HAL_RCC_I2C1_FORCE_RESET()
#define rI2C1_RELEASE_RESET() __HAL_RCC_I2C1_RELEASE_RESET()
#define rI2C1_SCL_PIN GPIO_PIN_6
#define rI2C1_SCL_GPIO_PORT GPIOB
#define rI2C1_SDA_PIN GPIO_PIN_7
#define rI2C1_SDA_GPIO_PORT GPIOB

#elif defined(RULOS_ARM_stm32f3)
#define RULOS_I2C_V2
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
#define RULOS_I2C_V2
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

/*
 * LL_I2C_DMA_GetRegAddr has a different signature on V1 vs V2: V1 has
 * one DR register for both send and receive, V2 has separate TXDR/RXDR
 * and wants a selector. Wrap the difference here so the call sites
 * don't need to care.
 */
#if defined(RULOS_I2C_V1)
#define rI2C1_DMA_TX_REG_ADDR() \
  ((volatile void *)(uintptr_t)LL_I2C_DMA_GetRegAddr(I2C1))
#define rI2C1_DMA_RX_REG_ADDR() \
  ((volatile void *)(uintptr_t)LL_I2C_DMA_GetRegAddr(I2C1))
#else
#define rI2C1_DMA_TX_REG_ADDR()                        \
  ((volatile void *)(uintptr_t)LL_I2C_DMA_GetRegAddr(  \
      I2C1, LL_I2C_DMA_REG_DATA_TRANSMIT))
#define rI2C1_DMA_RX_REG_ADDR()                        \
  ((volatile void *)(uintptr_t)LL_I2C_DMA_GetRegAddr(  \
      I2C1, LL_I2C_DMA_REG_DATA_RECEIVE))
#endif

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
  uint32_t packets_sent;
  uint32_t packets_nacked;
  uint32_t send_errors;

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
#if defined(RULOS_I2C_V1)
  LL_I2C_EnableReset(I2C1);
  LL_I2C_DisableReset(I2C1);
#elif defined(RULOS_I2C_V2)
  LL_I2C_Disable(I2C1);
  LL_I2C_Enable(I2C1);
#else
#include <stophere>
#endif
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

#if defined(RULOS_I2C_V1)
static void twi_generate_stop(TwiState *twi) {
  LL_I2C_GenerateStopCondition(I2C1);

  // The ST I2C application note AN2824 says you're supposed to wait
  // for STOP to be cleared by the hardware after setting it.
  // https://www.st.com/content/ccc/resource/technical/document/application_note/5d/ae/a3/6f/08/69/4e/9b/CD00209826.pdf/files/CD00209826.pdf/jcr:content/translations/en.CD00209826.pdf
  // I've noticed that sometimes, under heavy load, this waits
  // forever. I'm not sure why. So we time out.
  int timeout = 1000;
  while (timeout > 0 && READ_BIT(I2C1->CR1, I2C_CR1_STOP)) {
    timeout--;
  }
  if (timeout == 0) {
    reset_bus(twi);
  }

  twi_send_done_upcall(twi);
}

// Send event interrupts for I2C V1. Warning: runs in interrupt context.
static void twi_send_event_interrupt(TwiState *twi) {
  // TODO: make I2C1 a constant inside TwiState if we ever have a
  // second TWI interface
  if (LL_I2C_IsActiveFlag_SB(I2C1)) {
    // Start has been transmitted. Transmit the target address.
    LL_I2C_TransmitData8(I2C1, twi->send_addr);
  } else if (LL_I2C_IsActiveFlag_ADDR(I2C1)) {
    // Address has been acknowledged. Enable the DMA transfer for the
    // outgoing data.
    LL_I2C_EnableDMAReq_TX(I2C1);
    LL_I2C_ClearFlag_ADDR(I2C1);
  } else if (LL_I2C_IsActiveFlag_BTF(I2C1) || LL_I2C_IsActiveFlag_TXE(I2C1)) {
    // After DMA has been disabled, BTF or TXE indicates the hardware
    // is ready for the next outgoing byte. We can either respond with
    // data or a STOP to indicate there's no more. We send a stop.
    twi_generate_stop(twi);
  }
}

// Send error interrupts for I2C V1. Warning: runs in interrupt context.
static void twi_send_error_interrupt(TwiState *twi) {
  // Generate a stop in case of any error.
  twi_generate_stop(twi);
}

#elif defined(RULOS_I2C_V2)

// Send event interrupts for I2C V2. Warning: runs in interrupt context.
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
  // V2 uses autostop, so generate send-done upcall; no need to
  // generate a stop.
  twi->send_errors++;
  twi_send_done_upcall(twi);
}
#else
#include <stophere>
#endif

// DMA TC callback for the I2C TX channel. Disables the peripheral's
// DMA request so that a BTF event is generated (V1) or the autostop
// sequence completes (V2).
static void twi_tx_dma_tc_callback(void *user_data) {
  (void)user_data;
  LL_I2C_DisableDMAReq_TX(I2C1);
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

  assert(!MASTER_ACTIVE(twi));

  rulos_dma_start(twi->tx_dma_ch, rI2C1_DMA_TX_REG_ADDR(), (void *)data, len);
#ifdef RULOS_I2C_V2
  // V2 enables DMA immediately because it auto-transmits the
  // address. V1 only enables DMA once address has been transmitted.
  LL_I2C_EnableDMAReq_TX(I2C1);
#endif

#ifdef RULOS_I2C_V1
  LL_I2C_DisableBitPOS(I2C1);
#endif

#if 0
  // Hardware bug fix? Sometimes stop isn't cleared.
  CLEAR_BIT(I2C1->CR1, I2C_CR1_STOP);
#endif

  // Wait for the current transaction to be complete, if any. Then,
  // atomically enable master mode.
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  while (LL_I2C_IsActiveFlag_BUSY(I2C1)) {
    hal_end_atomic(old_interrupts);
    __WFI();
    old_interrupts = hal_start_atomic();
  }

  twi->send_done_cb = send_done_cb;
  twi->send_done_cb_data = send_done_cb_data;
  twi->send_addr = dest_addr << 1;  // add "write" bit
  LL_I2C_AcknowledgeNextData(I2C1, LL_I2C_ACK);

#if defined(RULOS_I2C_V1)
  LL_I2C_GenerateStartCondition(I2C1);
#elif defined(RULOS_I2C_V2)
  LL_I2C_HandleTransfer(I2C1, twi->send_addr, LL_I2C_ADDRSLAVE_7BIT, len,
                        LL_I2C_MODE_AUTOEND, LL_I2C_GENERATE_START_WRITE);
#else
#include <stophere>
#endif
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
#if defined(RULOS_I2C_V1)
  LL_RCC_ClocksTypeDef rcc_clocks;
  LL_RCC_GetSystemClocksFreq(&rcc_clocks);
  LL_I2C_ConfigSpeed(I2C1, rcc_clocks.PCLK1_Frequency, speed_khz * 1000,
                     LL_I2C_DUTYCYCLE_2);
#elif defined(RULOS_I2C_V2)
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1);
  LL_RCC_SetI2CClockSource(LL_RCC_I2C1_CLKSOURCE_SYSCLK);
  // I can't figure out from the datasheet how to adjust these numbers
  // from first principles to make them correct. I'm kinda unhappy
  // with the STM32 I2C V2 peripheral's timing setup; the datasheet is
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
#else
#include <stophere>
#error "timing for i2c needed"
#endif

  LL_I2C_SetOwnAddress1(I2C1, local_addr << 1, LL_I2C_OWNADDRESS1_7BIT);
#ifdef RULOS_I2C_V2
  LL_I2C_EnableOwnAddress1(I2C1);
#endif
  LL_I2C_EnableClockStretching(I2C1);
  LL_I2C_SetMode(I2C1, LL_I2C_MODE_I2C);

  // Turn on interrupts
  NVIC_SetPriority(I2C1_ER_IRQn, 0x2);
  NVIC_EnableIRQ(I2C1_ER_IRQn);
  NVIC_SetPriority(I2C1_EV_IRQn, 0x2);
  NVIC_EnableIRQ(I2C1_EV_IRQn);

#if defined(RULOS_I2C_V1)
  LL_I2C_EnableIT_EVT(I2C1);
#elif defined(RULOS_I2C_V2)
  LL_I2C_EnableIT_STOP(I2C1);
  LL_I2C_EnableIT_ADDR(I2C1);
#else
#include <stophere>
#endif
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

  if (MASTER_ACTIVE(twi)) {
    twi_send_event_interrupt(twi);
  } else {
    twi_recv_event_interrupt(twi);
  }
}

void I2C1_ER_IRQHandler(void) {
  TwiState *twi = &g_twi[0];

  // In the future, if the send-done callback wants to differentiate
  // between different kinds of error, it can be recorded here and
  // passed into twi_send_done.
  if (LL_I2C_IsActiveFlag_BERR(I2C1)) {
    // Bus error. Reset the I2C interface in hopes of recovering.
    LL_I2C_ClearFlag_BERR(I2C1);
    reset_bus(twi);
  }
  if (LL_I2C_IsActiveFlag_ARLO(I2C1)) {
    // Arbitration lost, meaning we couldn't acquire the bus.
    LL_I2C_ClearFlag_ARLO(I2C1);
  }
#ifdef RULOS_I2C_V1
  if (LL_I2C_IsActiveFlag_AF(I2C1)) {
    // Destination address didn't ack when we sent their address.
    LL_I2C_ClearFlag_AF(I2C1);
  }
#endif
  if (LL_I2C_IsActiveFlag_OVR(I2C1)) {
    LL_I2C_ClearFlag_OVR(I2C1);
  }

  if (MASTER_ACTIVE(twi)) {
    twi_send_error_interrupt(twi);
  } else {
    twi_recv_error_interrupt(twi);
  }
}
