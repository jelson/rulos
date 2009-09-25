#ifndef numeric_input_h
#define numeric_input_h

#include "clock.h"
#include "board_buffer.h"
#include "focus.h"
#include "region.h"

typedef struct {
	uint16_t mantissa;
	uint8_t neg_exponent;	// base 10
} DecimalFloatingPoint;

uint8_t dfp_draw(DecimalFloatingPoint *dfp, SSBitmap *bm, uint8_t len, uint8_t show_decimal);

struct s_numeric_input_act;
typedef struct {
	UIEventHandlerFunc func;
	struct s_numeric_input_act *act;
} NumericInputHandler;

typedef struct s_numeric_input_act {
	//ActivationFunc func;
	RowRegion region;
	NumericInputHandler handler;
	CursorAct cursor;
	DecimalFloatingPoint old_value;
	DecimalFloatingPoint cur_value;
	uint8_t decimal_present;
} NumericInputAct;

void numeric_input_init(NumericInputAct *act, RowRegion region, FocusManager *fa);

#endif // numeric_input_h
