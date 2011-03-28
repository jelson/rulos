#ifndef _LED_MATRIX_SINGLE_H
#define _LED_MATRIX_SINGLE_H

#include "rocket.h"

typedef struct {
	uint8_t red[8];
	uint8_t green[8];
} LMSBitmap;

typedef struct {
	LMSBitmap bitmap;
	uint8_t row;
	r_bool pwm_enable;
} LEDMatrixSingle;

void led_matrix_single_init(LEDMatrixSingle *lms, uint8_t timer_id);
void _lms_configure_row(uint8_t rowdata);
void _lms_configure_col(uint16_t coldata);
void _lms_configure(uint8_t rowdata, uint16_t coldata);
void lms_draw(LEDMatrixSingle *lms, LMSBitmap *bitmap);
void lms_enable(LEDMatrixSingle *lms, r_bool enable);

#endif // _LED_MATRIX_SINGLE_H
