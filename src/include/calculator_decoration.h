#ifndef _calculator_decoration_h
#define _calculator_decoration_h

#include "rocket.h"
#include "numeric_input.h"

struct s_decoration_state;
typedef void (*FetchCalcDecorationValuesFunc)(
	struct s_decoration_state *state,
	DecimalFloatingPoint *op0, DecimalFloatingPoint *op1);
typedef struct s_decoration_state {
	FetchCalcDecorationValuesFunc func;
} FetchCalcDecorationValuesIfc;

#endif // _calculator_decoration_h
