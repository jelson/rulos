#ifndef display_compass_h
#define display_compass_h

#include "clock.h"
#include "board_buffer.h"

typedef struct {
	ActivationFunc func;
	BoardBuffer bbuf;
	uint8_t offset;
} DCompassAct;

void dcompass_init(DCompassAct *act, uint8_t board);

#endif // display_compass_h
