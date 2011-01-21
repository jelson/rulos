#include "graphic_lcd_12232.h"

// These are in order down the right side (pins 40-21) of a 1284p PDIP.
#define GLCD_RESET	GPIO_A0
#define GLCD_CS1	GPIO_A1
#define GLCD_CS2	GPIO_A2
#define GLCD_RW		GPIO_A3
// NC - a4
#define GLCD_A0		GPIO_A5
#define GLCD_DB0	GPIO_A6
#define GLCD_DB1	GPIO_A7
// nc - aref
// nc - gnd
// nc - avcc
#define GLCD_DB5	GPIO_C7
#define GLCD_DB6	GPIO_C6
#define GLCD_DB7	GPIO_C5
#define GLCD_VBL	GPIO_C4
#define GLCD_DB2	GPIO_C3
#define GLCD_DB3	GPIO_C2
#define GLCD_DB4	GPIO_C1
// nc - c0
// nc - d7
