/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terr6l of the GNU General Public License as published by
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

#include "periph/rocket6line/rocket6line.h"
#include "core/hardware.h"
#include "core/rulos.h"
#include "core/stats.h"
#include "periph/7seg_panel/display_controller.h"

#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_rcc.h"
#include "stm32f3xx_hal_rcc_ex.h"
#include "stm32f3xx_hal_spi.h"
#include "stm32f3xx_ll_spi.h"
#include "stm32f3xx_ll_tim.h"

#define PRINT_STATS 0
#define USE_HAL 0

typedef struct {
  SPI_HandleTypeDef hspi;
  int curr_row;
  uint8_t row_data[ROCKET6LINE_NUM_ROWS][ROCKET6LINE_NUM_COLUMNS];
#if PRINT_STATS
  MinMaxMean_t refresh_time_stats;
#endif
  bool initted;
} Rocket6Line_t;

// Since we're driving the 6-line matrix from a global timer (interrupt
// handler), there's a single, global object.
static Rocket6Line_t r6l_g;

///////////////////////////////////////////////////////////////////////

#if defined(ROCKET6LINE_REV_A)
// Decoder pin definitions
#define DECODER_ENABLE GPIO_A3
#define DECODER_A0 GPIO_A0
#define DECODER_A1 GPIO_A1
#define DECODER_A2 GPIO_A2
#elif defined(ROCKET6LINE_REV_B)
#define ROW0_ENABLE GPIO_A0
#define ROW1_ENABLE GPIO_A1
#define ROW2_ENABLE GPIO_A2
#define ROW3_ENABLE GPIO_A3
#define ROW4_ENABLE GPIO_A4
#define ROW5_ENABLE GPIO_A5
#else
#error "Which kind of 6line hardware do you have?"
#include <stophere>
#endif

// LED driver pin definitions
#define LEDDRIVER_LE GPIO_A7
#define LEDDRIVER_OE GPIO_A6

#define LEDDRIVER_SDI GPIO_B5
#define LEDDRIVER_SDI_PORT GPIOB
#define LEDDRIVER_SDI_PIN GPIO_PIN_5

#define LEDDRIVER_CLK GPIO_B3
#define LEDDRIVER_CLK_PORT GPIOB
#define LEDDRIVER_CLK_PIN GPIO_PIN_3

#define LEDDRIVER_SPI_AF GPIO_AF5_SPI1

#define DEBUG_PIN GPIO_A15

static void init_pins(Rocket6Line_t *r6l) {
  // debug
  gpio_make_output(DEBUG_PIN);

#if defined(ROCKET6LINE_REV_A)
  // Set up decoder
  gpio_make_output(DECODER_ENABLE);
  gpio_make_output(DECODER_A0);
  gpio_make_output(DECODER_A1);
  gpio_make_output(DECODER_A2);
  gpio_clr(DECODER_ENABLE);
#elif defined(ROCKET6LINE_REV_B)
  gpio_make_output(ROW0_ENABLE);
  gpio_make_output(ROW1_ENABLE);
  gpio_make_output(ROW2_ENABLE);
  gpio_make_output(ROW3_ENABLE);
  gpio_make_output(ROW4_ENABLE);
  gpio_make_output(ROW5_ENABLE);
#endif

  // Set up output-enable and latch-enable pins of LED driver
  gpio_make_output(LEDDRIVER_LE);
  gpio_make_output(LEDDRIVER_OE);
  gpio_clr(LEDDRIVER_OE);

  // Configure SPI clock pin for LED driver
  GPIO_InitTypeDef gpio;
  memset(&gpio, 0, sizeof(gpio));
  gpio.Pin = LEDDRIVER_CLK_PIN;
  gpio.Alternate = LEDDRIVER_SPI_AF;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LEDDRIVER_CLK_PORT, &gpio);

  // Configure SPI data pin for LED driver
  gpio.Pin = LEDDRIVER_SDI_PIN;
  HAL_GPIO_Init(LEDDRIVER_SDI_PORT, &gpio);

  __HAL_RCC_SPI1_CLK_ENABLE();

  memset(&r6l->hspi, 0, sizeof(r6l->hspi));
  r6l->hspi.Instance = SPI1;
  r6l->hspi.Init.Mode = SPI_MODE_MASTER;
  r6l->hspi.Init.Direction = SPI_DIRECTION_1LINE;
  r6l->hspi.Init.DataSize = SPI_DATASIZE_8BIT;
  r6l->hspi.Init.CLKPolarity = SPI_POLARITY_LOW;
  r6l->hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
  r6l->hspi.Init.NSS = SPI_NSS_HARD_OUTPUT;
  r6l->hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  r6l->hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
  r6l->hspi.Init.TIMode = SPI_TIMODE_DISABLE;
  r6l->hspi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  r6l->hspi.Init.CRCPolynomial = 10;
  HAL_SPI_Init(&r6l->hspi);
  __HAL_SPI_ENABLE(&r6l->hspi);
  SPI_1LINE_TX(&r6l->hspi);

  // Initialize the refresh timer
  __HAL_RCC_TIM2_CLK_ENABLE();
  HAL_NVIC_SetPriority(TIM2_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);
  LL_TIM_InitTypeDef timer_init;
  timer_init.Prescaler = 0;
  timer_init.CounterMode = LL_TIM_COUNTERMODE_UP;
  timer_init.Autoreload = 3200;
  timer_init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  timer_init.RepetitionCounter = 0;
  LL_TIM_Init(TIM2, &timer_init);
  LL_TIM_SetTriggerOutput(TIM2, LL_TIM_TRGO_UPDATE);
  LL_TIM_DisableMasterSlaveMode(TIM2);
  LL_TIM_EnableIT_UPDATE(TIM2);
  LL_TIM_EnableCounter(TIM2);
  LL_TIM_GenerateEvent_UPDATE(TIM2);
}

static uint8_t segment_remap_table[256];

// Generate a remap table from RULOS "SSBitmap" to one bitmap on this board.
static void build_remap_table_one_digit(uint8_t *out, SSBitmap *bm) {
  *out = 0;
  if (*bm & (1 << 0)) {
    *out |= (1 << 6);
  }
  if (*bm & (1 << 1)) {
    *out |= (1 << 5);
  }
  if (*bm & (1 << 2)) {
    *out |= (1 << 4);
  }
  if (*bm & (1 << 3)) {
    *out |= (1 << 3);
  }
  if (*bm & (1 << 4)) {
    *out |= (1 << 2);
  }
  if (*bm & (1 << 5)) {
    *out |= (1 << 1);
  }
  if (*bm & (1 << 6)) {
    *out |= (1 << 0);
  }
  if (*bm & (1 << 7)) {
    *out |= (1 << 7);
  }
}

// Build the remap table from every possible SSBitmap to its corresponding
// output on this board.
static void build_remap_table() {
  uint8_t i = 0;
  do {
    build_remap_table_one_digit(&segment_remap_table[i], &i);
    i++;
  } while (i != 0);
}

#if defined(ROCKET6LINE_REV_A)
static inline void disable_all_rows() { gpio_clr(DECODER_ENABLE); }

static inline void enable_row(const uint8_t row_num) {
  // Configure the current row display in the decoder
  gpio_set_or_clr(DECODER_A0, row_num & (1 << 0));
  gpio_set_or_clr(DECODER_A1, row_num & (1 << 1));
  gpio_set_or_clr(DECODER_A2, row_num & (1 << 2));

  // Re-enable the encoder
  gpio_set(DECODER_ENABLE);
}

#elif defined(ROCKET6LINE_REV_B)

static inline void disable_all_rows() {
  gpio_set(ROW0_ENABLE);
  gpio_set(ROW1_ENABLE);
  gpio_set(ROW2_ENABLE);
  gpio_set(ROW3_ENABLE);
  gpio_set(ROW4_ENABLE);
  gpio_set(ROW5_ENABLE);
}

static inline void enable_row(const uint8_t row_num) {
  switch (row_num) {
    case 0:
      gpio_clr(ROW0_ENABLE);
      break;
    case 1:
      gpio_clr(ROW1_ENABLE);
      break;
    case 2:
      gpio_clr(ROW2_ENABLE);
      break;
    case 3:
      gpio_clr(ROW3_ENABLE);
      break;
    case 4:
      gpio_clr(ROW4_ENABLE);
      break;
    case 5:
      gpio_clr(ROW5_ENABLE);
      break;
  }
}
#endif

// This is the high-update-rate path that drives the persistence-of-vision of
// the matrix display. It's important this run quickly!
static void refresh_display(Rocket6Line_t *r6l) {
  r6l->curr_row++;
  if (r6l->curr_row == ROCKET6LINE_NUM_ROWS) {
    r6l->curr_row = 0;
  }

  // gpio_clr(DEBUG_PIN);

  // Shift in the LED configuration for the row becoming active
#if USE_HAL
  HAL_SPI_Transmit(&r6l->hspi, r6l->row_data[r6l->curr_row],
                   ROCKET6LINE_NUM_COLUMNS, 1000);
#else
  // Transmit the bytes two-at-a-time to reduce overhead.
  for (int i = 0; i < ROCKET6LINE_NUM_COLUMNS; i += 2) {
    // Wait until there's space in the FIFO.
    do {
    } while (!LL_SPI_IsActiveFlag_TXE(SPI1));

    // Send 2 bytes to the peripheral.
    LL_SPI_TransmitData16(SPI1,
                          *((uint16_t *)&r6l->row_data[r6l->curr_row][i]));
  }
#endif
  // gpio_set(DEBUG_PIN);

  // Disable current display line.
  disable_all_rows();

  // Wait for the bits to be shifted in completely, i.e. the SPI
  // peripheral has drained its queue.
  do {
  } while (LL_SPI_IsActiveFlag_BSY(SPI1));

  // Latch in the shifted data.
  gpio_set(LEDDRIVER_LE);
  gpio_clr(LEDDRIVER_LE);

  enable_row(r6l->curr_row);
}

// Timer callback that fires at the update rate (~10khz).
void TIM2_IRQHandler() {
  if (LL_TIM_IsActiveFlag_UPDATE(TIM2)) {
    LL_TIM_ClearFlag_UPDATE(TIM2);

    gpio_set(DEBUG_PIN);

#if PRINT_STATS
    Time start = precise_clock_time_us();
#endif

    refresh_display(&r6l_g);

#if PRINT_STATS
    Time elapsed = precise_clock_time_us() - start;
    minmax_add_sample(&r6l_g.refresh_time_stats, elapsed);
#endif
    gpio_clr(DEBUG_PIN);
  }
}

// If we're collecting statistics, print a stats summary once per second.
#if PRINT_STATS
static void rocket6line_print_stats(void *data) {
  schedule_us(1000000, rocket6line_print_stats, data);
  Rocket6Line_t *r6l = (Rocket6Line_t *)data;

  minmax_log(&r6l->refresh_time_stats, "r6l refresh");
  minmax_init(&r6l->refresh_time_stats);
}
#endif

void rocket6line_init() {
  memset(&r6l_g, 0, sizeof(r6l_g));

#if PRINT_STATS
  minmax_init(&r6l_g.refresh_time_stats);
  schedule_us(1000000, rocket6line_print_stats, &r6l_g);
#endif

  build_remap_table();
  init_pins(&r6l_g);
  r6l_g.initted = true;
}

void rocket6line_write_digit(const uint8_t row_num, const uint8_t col_num,
                             const SSBitmap bm) {
  assert(r6l_g.initted && row_num >= 0 && row_num < ROCKET6LINE_NUM_ROWS &&
         col_num >= 0 && col_num < ROCKET6LINE_NUM_COLUMNS);

  r6l_g.row_data[row_num][ROCKET6LINE_NUM_COLUMNS - col_num - 1] =
      segment_remap_table[bm];
}

void rocket6line_write_line(const uint8_t row_num, const SSBitmap *const bm,
                            const uint8_t len) {
  for (int col_num = 0; col_num < len; col_num++) {
    rocket6line_write_digit(row_num, col_num, bm[col_num]);
  }
}
