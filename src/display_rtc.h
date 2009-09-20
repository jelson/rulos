#ifndef display_rtc_h
#define display_rtc_h

#include "clock.h"
#include "board_buffer.h"

typedef struct {
	ActivationFunc func;
	BoardBuffer bbuf;
} DRTCAct;

void drtc_init(DRTCAct *act, uint8_t board);

#endif // display_rtc_h
