#ifndef __rocket_h__
#define __rocket_h__


#include "inttypes.h"


/*
 * Functions that are separately implemented by both the simulator and the real hardware
 */

typedef void (*Handler)();

char scan_keyboard();
void delay_ms(int ms);
void start_clock_ms(int ms, Handler handler);

/*
 * sim only
 */
void sim_init();
void sim_run();

/*
 * hardware only
 */
void hw_init();
void hw_run();


#endif /* __rocket_h__ */


