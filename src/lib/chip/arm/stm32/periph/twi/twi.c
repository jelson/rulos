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
 * STM32 TWI support
 */

#include <string.h>

#include "core/hardware.h"
#include "core/hardware_types.h"
#include "core/rulos.h"

#if defined(RULOS_ARM_stm32f0)
#include "stm32f0xx_ll_dma.h"
#include "stm32f0xx_ll_i2c.h"
#include "stm32f0xx_ll_rcc.h"

#elif defined(RULOS_ARM_stm32f1)
#include "stm32f1xx_ll_dma.h"
#include "stm32f1xx_ll_i2c.h"
#include "stm32f1xx_ll_rcc.h"

#define rI2C1_CLK_ENABLE() __HAL_RCC_I2C1_CLK_ENABLE()
#define rI2C1_SDA_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define rI2C1_SCL_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define rI2C1_DMA_CLK_ENABLE() __HAL_RCC_DMA1_CLK_ENABLE()
#define rI2C1_FORCE_RESET() __HAL_RCC_I2C1_FORCE_RESET()
#define rI2C1_RELEASE_RESET() __HAL_RCC_I2C1_RELEASE_RESET()
#define rI2C1_SCL_PIN GPIO_PIN_6
#define rI2C1_SCL_GPIO_PORT GPIOB
#define rI2C1_SDA_PIN GPIO_PIN_7
#define rI2C1_SDA_GPIO_PORT GPIOB
#define rI2C1_DMA DMA1
#define rI2C1_DMA_TX_CHAN LL_DMA_CHANNEL_6
#define rI2C1_DMA_TX_IRQn DMA1_Channel6_IRQn
#define rI2C1_DMA_TX_IRQHandler DMA1_Channel6_IRQHandler
#define rI2C1_DMA_TX_ClearTCFlag() LL_DMA_ClearFlag_TC6(DMA1)
#define rI2C1_DMA_RX_CHAN LL_DMA_CHANNEL_7
#define rI2C1_DMA_RX_IRQn DMA1_Channel7_IRQn
#define rI2C1_DMA_RX_IRQHandler DMA1_Channel7_IRQHandler
#define rI2C1_DMA_RX_ClearTCFlag() LL_DMA_ClearFlag_TC7(DMA1)
#else
#error "Your chip's definitions for I2C registers could use some help."
#include <stophere>
#endif

typedef struct {
  MediaStateIfc media;

  // sending
  MediaSendDoneFunc send_done_cb;
  void *send_done_cb_data;
  uint8_t send_addr;

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
  LL_I2C_EnableReset(I2C1);
  LL_I2C_DisableReset(I2C1);
}

/////////////////////// sending /////////////////////////////////////////

// Warning: runs in interrupt context.
static void twi_recv_done(TwiState *twi) {
  // Real packet received!
  assert(twi->slaveRecvSlot != NULL);
  int packet_len = twi->slaveRecvSlot->capacity -
                   LL_DMA_GetDataLength(rI2C1_DMA, rI2C1_DMA_RX_CHAN);

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
    LL_DMA_ConfigAddresses(
        rI2C1_DMA, rI2C1_DMA_RX_CHAN, LL_I2C_DMA_GetRegAddr(I2C1),
        (uint32_t)&twi->slaveRecvSlot->data[0],
        LL_DMA_GetDataTransferDirection(rI2C1_DMA, rI2C1_DMA_RX_CHAN));
    LL_DMA_SetDataLength(rI2C1_DMA, rI2C1_DMA_RX_CHAN,
                         twi->slaveRecvSlot->capacity);
    LL_DMA_EnableChannel(rI2C1_DMA, rI2C1_DMA_RX_CHAN);
    LL_I2C_EnableDMAReq_RX(I2C1);
    LL_I2C_ClearFlag_ADDR(I2C1);
  } else if (LL_I2C_IsActiveFlag_STOP(I2C1)) {
    // Stop detected. Clear the stop flag and run the receive handler.
    LL_I2C_ClearFlag_STOP(I2C1);
    LL_I2C_DisableDMAReq_RX(I2C1);
    LL_DMA_DisableChannel(rI2C1_DMA, rI2C1_DMA_RX_CHAN);
    twi_recv_done(twi);
  }
}

// Warning: runs in interrupt context.
static void twi_recv_error_interrupt(TwiState *twi) {
  LL_I2C_DisableDMAReq_RX(I2C1);
}

void I2C1_DMA_RX_IRQHandler(void) {
  // The only time we should ever get an interrupt from the RX channel
  // of the DMA controller is if the receive buffer fills or there's
  // an error. Either way, we should stop DMA and no longer ACK any
  // futher data that's received.
  rI2C1_DMA_RX_ClearTCFlag();
  LL_I2C_DisableDMAReq_RX(I2C1);
  LL_I2C_AcknowledgeNextData(I2C1, LL_I2C_NACK);
}

/////////////////////// sending /////////////////////////////////////////

#define MASTER_ACTIVE(twi) ((twi)->send_done_cb != NULL)

// Warning: runs in interrupt context.
static void twi_send_done(TwiState *twi) {
  assert(MASTER_ACTIVE(twi));
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

  // NB: send_done_cb must be nulled before the callback runs, since
  // the callback is likely to want to send another packet. But,
  // schedule_now doesn't run the callback until the scheduler runs
  // again, so this works.
  schedule_now((ActivationFuncPtr)twi->send_done_cb, twi->send_done_cb_data);
  // schedule_us(100000, (ActivationFuncPtr)twi->send_done_cb,
  // twi->send_done_cb_data);
  twi->send_done_cb = NULL;
  twi->send_done_cb_data = NULL;
}

// Warning: runs in interrupt context.
static void twi_send_event_interrupt(TwiState *twi) {
  // TODO: make I2C1 a constant inside TwiState if we ever have a
  // second TWI interface
  if (LL_I2C_IsActiveFlag_SB(I2C1)) {
    // Start has been transmitted. Transmit the target address.
    LL_I2C_TransmitData8(I2C1, twi->send_addr << 1);
  } else if (LL_I2C_IsActiveFlag_ADDR(I2C1)) {
    // Address has been acknowledged. Enable the DMA transfer for the
    // outgoing data.
    LL_I2C_EnableDMAReq_TX(I2C1);
    LL_I2C_ClearFlag_ADDR(I2C1);
  } else if (LL_I2C_IsActiveFlag_BTF(I2C1) || LL_I2C_IsActiveFlag_TXE(I2C1)) {
    // After DMA has been disabled, BTF or TXE indicates the hardware
    // is ready for the next outgoing byte. We can either respond with
    // data or a STOP to indicate there's no more. We send a stop.
    twi_send_done(twi);
  }
}

// Warning: runs in interrupt context.
static void twi_send_error_interrupt(TwiState *twi) { twi_send_done(twi); }

// Interrupt called when either the transmit-side DMA is complete or
// encountered an error. Disable DMA for I2C so that a BTF event is
// generated.
void rI2C1_DMA_TX_IRQHandler(void) {
  rI2C1_DMA_TX_ClearTCFlag();
  LL_I2C_DisableDMAReq_TX(I2C1);
}

static void twi_send(MediaStateIfc *media, Addr dest_addr,
                     const unsigned char *data, uint8_t len,
                     MediaSendDoneFunc send_done_cb, void *send_done_cb_data) {
#ifdef TIMING_DEBUG_PIN
  gpio_set(TIMING_DEBUG_PIN);
#endif

  // If we ever support multiple TWI interfaces, add a lookup based on
  // id here.
  TwiState *twi = &g_twi[0];

  assert(!MASTER_ACTIVE(twi));

  LL_DMA_DisableChannel(rI2C1_DMA, rI2C1_DMA_TX_CHAN);
  LL_DMA_ConfigAddresses(
      rI2C1_DMA, rI2C1_DMA_TX_CHAN, (uint32_t)data, LL_I2C_DMA_GetRegAddr(I2C1),
      LL_DMA_GetDataTransferDirection(rI2C1_DMA, rI2C1_DMA_TX_CHAN));
  LL_DMA_SetDataLength(rI2C1_DMA, rI2C1_DMA_TX_CHAN, len);
  LL_DMA_EnableChannel(rI2C1_DMA, rI2C1_DMA_TX_CHAN);

  LL_I2C_DisableBitPOS(I2C1);

  // Hardware bug fix? Sometimes stop isn't cleared.
  CLEAR_BIT(I2C1->CR1, I2C_CR1_STOP);

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
  twi->send_addr = dest_addr;
  LL_I2C_AcknowledgeNextData(I2C1, LL_I2C_ACK);
  LL_I2C_GenerateStartCondition(I2C1);
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
  twi->media.send = &twi_send;
  twi->slaveRecvSlot = slaveRecvSlot;

  // Enable clocks
  rI2C1_SCL_GPIO_CLK_ENABLE();
  rI2C1_SDA_GPIO_CLK_ENABLE();
  rI2C1_CLK_ENABLE();
  rI2C1_DMA_CLK_ENABLE();

  reset_bus(twi);

  // Configure GPIO
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Pin = rI2C1_SCL_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(rI2C1_SCL_GPIO_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = rI2C1_SDA_PIN;
  HAL_GPIO_Init(rI2C1_SDA_GPIO_PORT, &GPIO_InitStruct);

  // Configure DMA TX channel
  LL_DMA_ConfigTransfer(rI2C1_DMA, rI2C1_DMA_TX_CHAN,
                        LL_DMA_DIRECTION_MEMORY_TO_PERIPH |
                            LL_DMA_PRIORITY_LOW | LL_DMA_MODE_NORMAL |
                            LL_DMA_PERIPH_NOINCREMENT |
                            LL_DMA_MEMORY_INCREMENT | LL_DMA_PDATAALIGN_BYTE |
                            LL_DMA_MDATAALIGN_BYTE);
  NVIC_SetPriority(rI2C1_DMA_TX_IRQn, 0x4);
  NVIC_EnableIRQ(rI2C1_DMA_TX_IRQn);

  // Enable transfer-complete and transfer-error interrupt generation for DMA.
  LL_DMA_EnableIT_TC(rI2C1_DMA, rI2C1_DMA_TX_CHAN);
  LL_DMA_EnableIT_TE(rI2C1_DMA, rI2C1_DMA_TX_CHAN);

  // Configure DMA RX channel
  LL_DMA_ConfigTransfer(rI2C1_DMA, rI2C1_DMA_RX_CHAN,
                        LL_DMA_DIRECTION_PERIPH_TO_MEMORY |
                            LL_DMA_PRIORITY_HIGH | LL_DMA_MODE_NORMAL |
                            LL_DMA_PERIPH_NOINCREMENT |
                            LL_DMA_MEMORY_INCREMENT | LL_DMA_PDATAALIGN_BYTE |
                            LL_DMA_MDATAALIGN_BYTE);
  NVIC_SetPriority(rI2C1_DMA_RX_IRQn, 0x1);
  NVIC_EnableIRQ(rI2C1_DMA_RX_IRQn);
  LL_DMA_EnableIT_TC(rI2C1_DMA, rI2C1_DMA_RX_CHAN);
  LL_DMA_EnableIT_TE(rI2C1_DMA, rI2C1_DMA_RX_CHAN);

  // Set up I2C
  LL_I2C_Disable(I2C1);
  LL_RCC_ClocksTypeDef rcc_clocks;
  LL_RCC_GetSystemClocksFreq(&rcc_clocks);
  LL_I2C_ConfigSpeed(I2C1, rcc_clocks.PCLK1_Frequency, speed_khz * 1000,
                     LL_I2C_DUTYCYCLE_2);
  LL_I2C_SetOwnAddress1(I2C1, local_addr << 1, LL_I2C_OWNADDRESS1_7BIT);
  LL_I2C_EnableClockStretching(I2C1);
  LL_I2C_SetMode(I2C1, LL_I2C_MODE_I2C);

  // Turn on interrupts
  NVIC_SetPriority(I2C1_ER_IRQn, 0x2);
  NVIC_EnableIRQ(I2C1_ER_IRQn);
  NVIC_SetPriority(I2C1_EV_IRQn, 0x2);
  NVIC_EnableIRQ(I2C1_EV_IRQn);
  LL_I2C_EnableIT_EVT(I2C1);
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
  if (LL_I2C_IsActiveFlag_AF(I2C1)) {
    // Destination address didn't ack when we sent their address.
    LL_I2C_ClearFlag_AF(I2C1);
  }
  if (LL_I2C_IsActiveFlag_OVR(I2C1)) {
    LL_I2C_ClearFlag_OVR(I2C1);
  }

  if (MASTER_ACTIVE(twi)) {
    twi_send_error_interrupt(twi);
  } else {
    twi_recv_error_interrupt(twi);
  }
}
