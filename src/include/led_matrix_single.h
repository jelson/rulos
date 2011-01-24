#ifndef _LED_MATRIX_SINGLE_H
#define _LED_MATRIX_SINGLE_H

#include "rocket.h"

typedef struct {
	
} LEDMatrixSingle;

void led_matrix_single_init(LEDMatrixSingle *lms);
void _lms_configure_row(uint8_t rowdata);
void _lms_configure_col(uint16_t coldata);
void _lms_configure(uint8_t rowdata, uint16_t coldata);

#endif // _LED_MATRIX_SINGLE_H
