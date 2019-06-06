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

#include "core/hardware.h"
#include "core/rulos.h"
#include "core/stats.h"
#include "core/twi.h"
#include "periph/7seg_panel/display_controller.h"
#include "periph/7seg_panel/remote_bbuf.h"
#include "periph/bss_canary/bss_canary.h"

#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_rcc.h"
#include "stm32f3xx_hal_rcc_ex.h"
#include "stm32f3xx_hal_spi.h"
#include "stm32f3xx_ll_tim.h"

#define REFRESH_RATE_US 2000
#define UPDATE_RATE_US 1000000

// For legacy backcompat we assume the first line of the matrix is index 3.
#define FIRST_LINE_IDX 3

#ifdef LOG_TO_SERIAL
HalUart uart;
#endif

#define TEST_MODE 0

///////////////////////////////////////////////////////////////////////

// Decoder pin definitions
#define DECODER_ENABLE GPIO_A3
#define DECODER_A0 GPIO_A0
#define DECODER_A1 GPIO_A1
#define DECODER_A2 GPIO_A2

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

#define NUM_ROWS 6
#define NUM_COLUMNS 8

typedef struct {
  SPI_HandleTypeDef hspi;
  int curr_row;
  uint8_t row_data[NUM_ROWS][NUM_COLUMNS];
  MinMaxMean_t refresh_time_stats;
  Network net;
  RemoteBBufRecv rbr;
} MatrixState_t;

MatrixState_t matrix_state;

static void init_pins(MatrixState_t *ms) {
  // debug
  gpio_make_output(DEBUG_PIN);

  // Set up decoder
  gpio_make_output(DECODER_ENABLE);
  gpio_make_output(DECODER_A0);
  gpio_make_output(DECODER_A1);
  gpio_make_output(DECODER_A2);
  gpio_clr(DECODER_ENABLE);

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

  memset(&ms->hspi, 0, sizeof(ms->hspi));
  ms->hspi.Instance = SPI1;
  ms->hspi.Init.Mode = SPI_MODE_MASTER;
  ms->hspi.Init.Direction = SPI_DIRECTION_1LINE;
  ms->hspi.Init.DataSize = SPI_DATASIZE_8BIT;
  ms->hspi.Init.CLKPolarity = SPI_POLARITY_LOW;
  ms->hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
  ms->hspi.Init.NSS = SPI_NSS_HARD_OUTPUT;
  ms->hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  ms->hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
  ms->hspi.Init.TIMode = SPI_TIMODE_DISABLE;
  ms->hspi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  ms->hspi.Init.CRCPolynomial = 10;
  HAL_SPI_Init(&ms->hspi);
  __HAL_SPI_ENABLE(&ms->hspi);

  // Initialize the refresh timer
  __HAL_RCC_TIM2_CLK_ENABLE();
  HAL_NVIC_SetPriority(TIM2_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);
  LL_TIM_InitTypeDef timer_init;
  timer_init.Prescaler = 0;
  timer_init.CounterMode = LL_TIM_COUNTERMODE_UP;
  timer_init.Autoreload = 6500;
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

static void convert_bitmap(uint8_t *out, SSBitmap *bm, const uint8_t len) {
  for (int i = 0; i < len; i++) {
    out[i] = segment_remap_table[bm[len - i - 1]];
  }
}

// This is the high-update-rate path that drives the persistence-of-vision of
// the matrix display. It's important this run quickly!
static void refresh_display(MatrixState_t *ms) {
  ms->curr_row++;
  if (ms->curr_row == NUM_ROWS) {
    ms->curr_row = 0;
  }

  // Shift in the LED configuration for the row becoming active
  HAL_SPI_Transmit(&ms->hspi, ms->row_data[ms->curr_row], NUM_COLUMNS, 1000);

  // Disable current display
  gpio_clr(DECODER_ENABLE);

  // Latch in the already-shifted data
  gpio_set(LEDDRIVER_LE);
  gpio_clr(LEDDRIVER_LE);

  // Configure the current row display in the decoder
  gpio_set_or_clr(DECODER_A0, ms->curr_row & (1 << 0));
  gpio_set_or_clr(DECODER_A1, ms->curr_row & (1 << 1));
  gpio_set_or_clr(DECODER_A2, ms->curr_row & (1 << 2));

  // Re-enable the display!
  gpio_set(DECODER_ENABLE);
}

void TIM2_IRQHandler() {
  if (LL_TIM_IsActiveFlag_UPDATE(TIM2)) {
    LL_TIM_ClearFlag_UPDATE(TIM2);

    gpio_set(DEBUG_PIN);
    Time start = precise_clock_time_us();
    refresh_display(&matrix_state);
    Time elapsed = precise_clock_time_us() - start;
    minmax_add_sample(&matrix_state.refresh_time_stats, elapsed);
    gpio_clr(DEBUG_PIN);
  }
}

#if TEST_MODE
static void test_mode_update(void *data) {
  schedule_us(UPDATE_RATE_US, (ActivationFuncPtr)test_mode_update, data);

  MatrixState_t *ms = (MatrixState_t *)data;

  minmax_log(&ms->refresh_time_stats, "refresh time");
  minmax_init(&ms->refresh_time_stats);

  SSBitmap bm[NUM_COLUMNS];
  char buf[NUM_COLUMNS + 2];
  static int test_state = 4;
  test_state++;
  if (test_state == 5) {
    test_state = 0;
  }
  switch (test_state) {
    case 0:
      strcpy(buf, "rrrrrrrr");
      ascii_to_bitmap_str(bm, strlen(buf), buf);
      for (int i = 0; i < NUM_ROWS; i++) {
        convert_bitmap(ms->row_data[i], bm, NUM_COLUMNS);
      }
      break;

    case 1:
      for (int i = 0; i < NUM_ROWS; i++) {
        sprintf(buf, "%d", i);
        ascii_to_bitmap_str(bm, 1, buf);
        for (int j = 0; j < NUM_COLUMNS; j++) {
          convert_bitmap(&ms->row_data[i][j], bm, 1);
        }
      }
      break;

    case 2:
      strcpy(buf, "CCCCCCCC");
      ascii_to_bitmap_str(bm, strlen(buf), buf);
      for (int i = 0; i < NUM_ROWS; i++) {
        convert_bitmap(ms->row_data[i], bm, NUM_COLUMNS);
      }
      break;

    case 3:
      strcpy(buf, "12345678");
      ascii_to_bitmap_str(bm, strlen(buf), buf);
      for (int i = 0; i < NUM_ROWS; i++) {
        convert_bitmap(ms->row_data[i], bm, NUM_COLUMNS);
      }
      break;

    case 4:
      for (int i = 0; i < NUM_ROWS; i++) {
        for (int j = 0; j < NUM_COLUMNS; j++) {
          ms->row_data[i][j] = 0xff;
        }
      }
      break;
  }
}
#endif  // TEST_MODE

static void matrix_bbuf_recv(MessageRecvBuffer *msg) {
  if (msg->payload_len != sizeof(BBufMessage)) {
    LOG("rbr_recv: Error: expected BBufMessage of size %zu, got %d bytes",
        sizeof(BBufMessage), msg->payload_len);
    goto done;
  }

  BBufMessage *bbm = (BBufMessage *)msg->data;
  MatrixState_t *ms = (MatrixState_t *)msg->app_receiver->user_data;

  if (bbm->index < FIRST_LINE_IDX || bbm->index >= FIRST_LINE_IDX + NUM_ROWS) {
    LOG("got unexpected row index %d", bbm->index);
    goto done;
  }

  convert_bitmap(ms->row_data[bbm->index - FIRST_LINE_IDX], bbm->buf,
                 NUM_DIGITS);

done:
  net_free_received_message_buffer(msg);
}

static void init_matrix_bbuf_recv(MatrixState_t *ms, RemoteBBufRecv *rbr,
                                  Network *network) {
  rbr->app_receiver.recv_complete_func = matrix_bbuf_recv,
  rbr->app_receiver.port = REMOTE_BBUF_PORT;
  rbr->app_receiver.num_receive_buffers = REMOTE_BBUF_RING_SIZE;
  rbr->app_receiver.payload_capacity = sizeof(BBufMessage);
  rbr->app_receiver.message_recv_buffers = rbr->recv_ring_alloc;
  rbr->app_receiver.user_data = ms;

  net_bind_receiver(network, &rbr->app_receiver);
}
int main() {
  hal_init();

#ifdef LOG_TO_SERIAL
  hal_uart_init(&uart, 115200, true, /* uart_id= */ 0);
  LOG("Log output running");
#endif

  init_clock(2000, TIMER1);
  bss_canary_init();

  memset(&matrix_state, 0, sizeof(matrix_state));
  build_remap_table();
  minmax_init(&matrix_state.refresh_time_stats);
  init_pins(&matrix_state);

#if TEST_MODE
  schedule_us(1, (ActivationFuncPtr)update_display_values, &matrix_state);
#else
  init_twi_network(&matrix_state.net, 100, DONGLE_BASE_ADDR + DONGLE_BOARD_ID);
  init_matrix_bbuf_recv(&matrix_state, &matrix_state.rbr, &matrix_state.net);
#endif

  cpumon_main_loop();
}
