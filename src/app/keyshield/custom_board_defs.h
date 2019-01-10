/*
 * The build system includes this file when compiling the OS if
 * BOARD := CUSTOM appears in the Makefile. That's what binds our
 * particular board's keypad pins to the RulOS keypad module.
 */

#if defined(FUTURLEC_KEYPAD)
#define KEYPAD_ROW0 /* pin 8 */ GPIO_A1
#define KEYPAD_ROW1 /* pin 1 */ GPIO_B2
#define KEYPAD_ROW2 /* pin 2 */ GPIO_B1
#define KEYPAD_ROW3 /* pin 4 */ GPIO_A7
#define KEYPAD_COL0 /* pin 3 */ GPIO_B0
#define KEYPAD_COL1 /* pin 5 */ GPIO_A5
#define KEYPAD_COL2 /* pin 6 */ GPIO_A3
#define KEYPAD_COL3 /* pin 7 */ GPIO_A2
#elif defined(OOTDTY_KEYPAD)
#define KEYPAD_ROW0 /* pin 7 */ GPIO_A2
#define KEYPAD_ROW1 /* pin 6 */ GPIO_A3
#define KEYPAD_ROW2 /* pin 5 */ GPIO_A5
#define KEYPAD_ROW3 /* pin 4 */ GPIO_A7
#define KEYPAD_COL0 /* pin 3 */ GPIO_B0
#define KEYPAD_COL1 /* pin 2 */ GPIO_B1
#define KEYPAD_COL2 /* pin 1 */ GPIO_B2
#define KEYPAD_COL3 /* pin 8 */ GPIO_A1
#else
#error "Unknown keypad"
#endif

#define OPT_PIN GPIO_A0
