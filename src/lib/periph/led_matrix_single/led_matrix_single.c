#include "periph/led_matrix_single/led_matrix_single.h"

#include <stdbool.h>

#define FINAL_STRIPBOARD 1
#if FINAL_STRIPBOARD

#define LMS_DATA_ROWS GPIO_D2
#define LMS_LATCH_ENABLE_COLS GPIO_D3
#define LMS_SHIFT_CLOCK_COLS GPIO_D4
#define LMS_DATA_COLS GPIO_D5
#define LMS_OUTPUT_ENABLE_INV GPIO_D6

#define LMS_SHIFT_CLOCK_ROWS GPIO_C0
#define LMS_LATCH_ENABLE_ROWS GPIO_D7

#else  // original breadboard
#define LMS_OUTPUT_ENABLE_INV GPIO_D6
#define LMS_DATA_COLS GPIO_D5
#define LMS_SHIFT_CLOCK_COLS GPIO_D4
#define LMS_LATCH_ENABLE_COLS GPIO_D3
#if 0
#define LMS_DATA_ROWS GPIO_D5
#define LMS_SHIFT_CLOCK_ROWS GPIO_D4
#define LMS_LATCH_ENABLE_ROWS GPIO_D2
#else
// separate control lines for rows
#define LMS_DATA_ROWS GPIO_C0         /* aux hdr 1 */
#define LMS_LATCH_ENABLE_ROWS GPIO_D7 /* aux hdr 2 */
#define LMS_SHIFT_CLOCK_ROWS GPIO_C1  /* aux hdr 3 */
#endif
#endif

void lms_shift_byte(uint8_t data);
void lms_configure(uint8_t rowdata, uint16_t coldata);

#if !SIM
#include "core/hardware.h"

// void lms_handler(void *arg);
void lms_update(LEDMatrixSingle *lms);

#define SYNCDEBUG() syncdebug(0, 'M', __LINE__)
void syncdebug(uint8_t spaces, char f, uint16_t line);

void led_matrix_single_init(LEDMatrixSingle *lms, uint8_t timer_id) {
  gpio_set(LMS_OUTPUT_ENABLE_INV);
  gpio_clr(LMS_DATA_COLS);
  gpio_clr(LMS_SHIFT_CLOCK_COLS);
  gpio_clr(LMS_LATCH_ENABLE_COLS);
  gpio_clr(LMS_DATA_ROWS);
  gpio_clr(LMS_SHIFT_CLOCK_ROWS);
  gpio_clr(LMS_LATCH_ENABLE_ROWS);
  gpio_make_output(LMS_OUTPUT_ENABLE_INV);
  gpio_make_output(LMS_DATA_COLS);
  gpio_make_output(LMS_SHIFT_CLOCK_COLS);
  gpio_make_output(LMS_LATCH_ENABLE_COLS);
  gpio_make_output(LMS_DATA_ROWS);
  gpio_make_output(LMS_SHIFT_CLOCK_ROWS);
  gpio_make_output(LMS_LATCH_ENABLE_ROWS);
  gpio_clr(LMS_OUTPUT_ENABLE_INV);

  memset(&lms->bitmap, 0, sizeof(lms->bitmap));
  lms->row = 0;

  SYNCDEBUG();
  lms->pwm_enable = true;
  //	hal_start_clock_us(1000000, &lms_handler, lms, timer_id);
  schedule_us(10000, (ActivationFuncPtr)lms_update, lms);
  SYNCDEBUG();

  lms_configure(0x20, 0x33f3);
}

//#define LMDELAY()	{volatile int x; for (int de=0; de<1000; de++) { x+=1;
//}}
#define LMDELAY()                    \
  {                                  \
    volatile int x;                  \
    for (int de = 0; de < 2; de++) { \
      x += 1;                        \
    }                                \
  }
//#define LMDELAY()	{}

inline static void lms_shift_byte_cols(uint8_t data) {
  uint8_t bit;
  for (bit = 0; bit < 8; bit++) {
    gpio_set_or_clr(LMS_DATA_COLS, data & 1);
    LMDELAY();
    gpio_set(LMS_SHIFT_CLOCK_COLS);
    LMDELAY();
    gpio_clr(LMS_SHIFT_CLOCK_COLS);
    LMDELAY();
    data = data >> 1;
  }
}

inline static void lms_shift_byte_rows(uint8_t data) {
  uint8_t bit;
  for (bit = 0; bit < 8; bit++) {
    gpio_set_or_clr(LMS_DATA_ROWS, data & 1);
    LMDELAY();
    gpio_clr(LMS_SHIFT_CLOCK_ROWS);
    LMDELAY();
    gpio_set(LMS_SHIFT_CLOCK_ROWS);
    LMDELAY();
    data = data >> 1;
  }
}

#define STROBE(LE) \
  {                \
    gpio_set(LE);  \
    LMDELAY();     \
    gpio_clr(LE);  \
    LMDELAY();     \
  }
#define ISTROBE(LE) \
  {                 \
    gpio_clr(LE);   \
    LMDELAY();      \
    gpio_set(LE);   \
    LMDELAY();      \
  }

void lms_configure_row(uint8_t rowdata) {
  lms_shift_byte_rows(rowdata);
  STROBE(LMS_LATCH_ENABLE_ROWS);
}

void lms_configure_col(uint16_t coldata) {
  lms_shift_byte_cols((coldata >> 8) & 0xff);
  lms_shift_byte_cols((coldata >> 0) & 0xff);
  STROBE(LMS_LATCH_ENABLE_COLS);
}

void lms_configure(uint8_t rowdata, uint16_t coldata) {
  //	gpio_set(LMS_OUTPUT_ENABLE_INV);
  lms_configure_row(rowdata);
  lms_configure_col(coldata);
  //	gpio_clr(LMS_OUTPUT_ENABLE_INV);
}

uint8_t lms_row_remap_table[8] = {1 << 3, 1 << 4, 1 << 5, 1 << 6,
                                  1 << 2, 1 << 1, 1 << 0, 1 << 7};
uint8_t lms_bitswap[16] = {0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
                           0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf};
// void lms_handler(void *arg)
void lms_update(LEDMatrixSingle *lms) {
  if (lms->pwm_enable) {
    // note that we draw a row's red and its green right after each
    // other, rather than a red frame and a row frame. That looks much
    // less flickery.
    r_bool do_green = lms->row & 1;
    uint8_t actual_row = lms->row >> 1;

    gpio_set(LMS_OUTPUT_ENABLE_INV);
    uint8_t row_val = lms_row_remap_table[actual_row];
    lms_configure_row(row_val);
    uint16_t col_val;
    if (do_green) {
      uint8_t green = lms->bitmap.green[actual_row];
      col_val = lms_bitswap[green & 0xf] | ((green & 0xf0) << 8);
    } else {
      uint8_t red = lms->bitmap.red[actual_row];
      col_val = (lms_bitswap[red & 0xf] << 4) | ((red & 0xf0) << 4);
    }
    lms_configure_col(col_val);
    gpio_clr(LMS_OUTPUT_ENABLE_INV);
    //		syncdebug(0, 'r', row_val);
    //		syncdebug(8, 'c', col_val);
    lms->row = (lms->row + 1) & 0xf;
  }
  schedule_us(400, (ActivationFuncPtr)lms_update, lms);
}

// Called to darken display when doing something slow
// (painting LCD), to avoid a stuck pixel.
// (Could split LCD paint into multiple continuations,
// or make LMS timer-driven, but no.
void lms_enable(LEDMatrixSingle *lms, r_bool enable) {
  gpio_set_or_clr(LMS_OUTPUT_ENABLE_INV, !enable);
}
#else  //! SIM
void led_matrix_single_init(LEDMatrixSingle *lms, uint8_t timer_id) {}
void lms_configure_row(uint8_t rowdata) {}
void lms_configure_col(uint16_t coldata) {}
void lms_enable(LEDMatrixSingle *lms, r_bool enable) {}

#endif  //! SIM

void lms_draw(LEDMatrixSingle *lms, LMSBitmap *bitmap) {
  memcpy(&lms->bitmap, bitmap, sizeof(LMSBitmap));
}
