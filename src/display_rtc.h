#ifndef display_rtc_h
#define display_rtc_h

#include "clock.h"
#include "board_buffer.h"

typedef struct {
	ActivationFunc func;
	Time base_time;
	BoardBuffer bbuf;
} DRTCAct;

void drtc_init(DRTCAct *act, uint8_t board, Time base_time);
void drtc_set_base_time(DRTCAct *act, Time base_time);

#endif // display_rtc_h
