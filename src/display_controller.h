#ifndef display_controller_h
#define display_controller_h

#include <inttypes.h>

#define NUM_BOARDS 6
#define NUM_DIGITS 8

/*
 * 7-segment shapes are defined as 7-bit numbers, each bit representing
 * a single segment.  From MSB to LSB (reading left to right),
 * we represent the 7 segments from A through G, like this:
 *   
 *    -a-
 *  f|   |b
 *    -g-
 *  e|   |c
 *    -d-    
 *
 * The decimal point is segment 7.
 */

#define SSB_DECIMAL 0x80
#define SSB_SEG_a 0x40
#define SSB_SEG_b 0x20
#define SSB_SEG_c 0x10
#define SSB_SEG_d 0x08
#define SSB_SEG_e 0x04
#define SSB_SEG_f 0x02
#define SSB_SEG_g 0x01

typedef uint8_t SSBitmap;

void init_pins();
void program_segment(uint8_t board, uint8_t digit, uint8_t segment, uint8_t onoff);
void program_cell(uint8_t board, uint8_t digit, SSBitmap bitmap);
void program_string(uint8_t board, char *string);

// debug routines: spray one character across a row or entire display
void program_row(uint8_t board, SSBitmap bitmap);
void program_matrix(SSBitmap bitmap);

SSBitmap ascii_to_bitmap(char a);

#endif // display_controller_h
