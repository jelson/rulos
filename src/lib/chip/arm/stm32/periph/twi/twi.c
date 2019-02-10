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

#if defined(RULOS_ARM_stm32f1)
#define I2C1_CLK_ENABLE() __HAL_RCC_I2C1_CLK_ENABLE()
#define I2C1_SDA_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define I2C1_SCL_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define I2C1_DMA_CLK_ENABLE() __HAL_RCC_DMA1_CLK_ENABLE()
#define I2C1_FORCE_RESET() __HAL_RCC_I2C1_FORCE_RESET()
#define I2C1_RELEASE_RESET() __HAL_RCC_I2C1_RELEASE_RESET()
#define I2C1_SCL_PIN GPIO_PIN_6
#define I2C1_SCL_GPIO_PORT GPIOB
#define I2C1_SDA_PIN GPIO_PIN_7
#define I2C1_SDA_GPIO_PORT GPIOB
#define I2C1_DMA DMA1
#define I2C1_DMA_INSTANCE_TX DMA1_Channel6
#define I2C1_DMA_INSTANCE_RX DMA1_Channel7
#define I2C1_DMA_TX_IRQn DMA1_Channel6_IRQn
#define I2C1_DMA_RX_IRQn DMA1_Channel7_IRQn
#define I2C1_DMA_TX_IRQHandler DMA1_Channel6_IRQHandler
#define I2C1_DMA_RX_IRQHandler DMA1_Channel7_IRQHandler
#define I2C1_EV_IRQn I2C1_EV_IRQn
#define I2C1_EV_IRQHandler I2C1_EV_IRQHandler
#define I2C1_ER_IRQn I2C1_ER_IRQn
#define I2C1_ER_IRQHandler I2C1_ER_IRQHandler
#else
#error "Your chip's definitions for I2C registers could use some help."
#include <stophere>
#endif

typedef struct {
  MediaStateIfc media;
  MediaSendDoneFunc send_done_cb;
  void *send_done_cb_data;
  I2C_HandleTypeDef hal_i2c_handle;
} TwiState;

// Right now the RULOS HAL interface doesn't even support multiple
// TWIs; I'm trying to structure the code in such a way that it won't
// be a violent change once it does.
#define NUM_TWI 1
static TwiState g_twi[NUM_TWI];

#define MASTER_ACTIVE(twi) ((twi)->send_done_cb != NULL)

// Warning: runs in interrupt context.
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) {
  TwiState *twi = &g_twi[0];

  assert(MASTER_ACTIVE(twi));
  // NB: send_done_cb must be nulled before the callback runs, since
  // the callback is likely to want to send another packet. But,
  // schedule_now doesn't run the callback until the scheduler runs
  // again, so this works.
  schedule_now((ActivationFuncPtr)twi->send_done_cb, twi->send_done_cb_data);
  twi->send_done_cb = NULL;
  twi->send_done_cb_data = NULL;
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
  assert(send_done_cb != NULL);

  while (HAL_I2C_GetState(&twi->hal_i2c_handle) != HAL_I2C_STATE_READY) {
    __WFI();
  }

  twi->send_done_cb = send_done_cb;
  twi->send_done_cb_data = send_done_cb_data;

  HAL_I2C_Master_Transmit(&twi->hal_i2c_handle, dest_addr << 1, (uint8_t *)data,
                          len, HAL_MAX_DELAY);

  while (HAL_I2C_GetState(&twi->hal_i2c_handle) != HAL_I2C_STATE_READY) {
    __WFI();
  }

  HAL_I2C_MasterTxCpltCallback(NULL);
#ifdef TIMING_DEBUG_PIN
  gpio_clr(TIMING_DEBUG_PIN);
#endif
}

MediaStateIfc *hal_twi_init(uint32_t speed_khz, Addr local_addr,
                            MediaRecvSlot *const slaveRecvSlot) {
  // Someday, if we make the HAL interface support multiple TWI
  // interfaces, this line should be changed from [0] to [index].
  TwiState *twi = &g_twi[0];
  memset(twi, 0, sizeof(TwiState));
  twi->media.send = &twi_send;

  I2C1_SCL_GPIO_CLK_ENABLE();
  I2C1_SDA_GPIO_CLK_ENABLE();
  I2C1_CLK_ENABLE();
  I2C1_FORCE_RESET();
  I2C1_RELEASE_RESET();

  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Pin = I2C1_SCL_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(I2C1_SCL_GPIO_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = I2C1_SDA_PIN;
  HAL_GPIO_Init(I2C1_SDA_GPIO_PORT, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(I2C1_ER_IRQn, 0, 1);
  HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
  HAL_NVIC_SetPriority(I2C1_EV_IRQn, 0, 2);
  HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);

  twi->hal_i2c_handle.Instance = I2C1;
  twi->hal_i2c_handle.Init.ClockSpeed = speed_khz * 1000;
  twi->hal_i2c_handle.Init.DutyCycle = I2C_DUTYCYCLE_2;
  twi->hal_i2c_handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  twi->hal_i2c_handle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  twi->hal_i2c_handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  twi->hal_i2c_handle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  twi->hal_i2c_handle.Init.OwnAddress1 = local_addr;
  twi->hal_i2c_handle.Init.OwnAddress2 = 0xFF;

  if (HAL_I2C_Init(&twi->hal_i2c_handle) != HAL_OK) {
    __builtin_trap();
  }

#ifdef TIMING_DEBUG_PIN
  gpio_make_output(TIMING_DEBUG_PIN);
#endif

  return &twi->media;
}
