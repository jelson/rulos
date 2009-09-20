#ifndef __rocket_h__
#define __rocket_h__


#include "inttypes.h"

/*
 * Functions that are separately implemented by both the simulator and the real hardware
 */

typedef void (*Handler)();

char scan_keyboard();
void _delay_ms();
void start_clock_ms(int ms, Handler handler);

void init_sim();
void init_hardware();


#endif /* __rocket_h__ */


