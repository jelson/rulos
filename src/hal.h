#ifndef _hal_h
#define _hal_h

void hal_init();
void hal_start_atomic();	// block interrupts/signals
void hal_end_atomic();		// resume interrupts/signals
void hal_idle();			// hw: spin. sim: sleep
char hal_scan_keyboard();

#endif // _hal_h
