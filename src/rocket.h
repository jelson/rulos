
/*
 * functions shared by both simulator and real program
 */

#include "inttypes.h"

char scan_keyboard();
void program_segment(uint8_t board, uint8_t digit, uint8_t segment, uint8_t onoff);
void _delay_ms();
void init_sim();

