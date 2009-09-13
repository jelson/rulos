#define NUM_BOARDS 4
#define NUM_DIGITS 8
#define NUM_SEGMENTS 8

#define DIGIT_WIDTH 5
#define DIGIT_HEIGHT 4

/*
   __
  |__|
  |__|.

seg  x  y char
0    1  0 __
1    3  1 |
2    3  2 |
3    1  2 __
4    0  2 |
5    0  1 |
6    1  1 __
7    4  2 .

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <curses.h>

#include "rocket.h"

static WINDOW *mainwnd;

struct segment_def_s {
  int xoff;
  int yoff;
  char *s;
} segment_defs[] =  {
  { 1, 0, "__" },
  { 3, 1, "|" },
  { 3, 2, "|" },
  { 1, 2, "__" },
  { 0, 2, "|" },
  { 0, 1, "|" },
  { 1, 1, "__" },
  { 4, 2, "." }
};

  
void init_sim()
{
  int board, digit, segment;
  mainwnd = initscr();
  noecho();
  cbreak();
  nodelay(mainwnd, TRUE);
  refresh(); // 1)
  wrefresh(mainwnd);
  curs_set(0);

#if 0
  for (board = 0; board < NUM_BOARDS; board++)
    for (digit = 0; digit < NUM_DIGITS; digit++)
      for (segment = 0; segment < NUM_SEGMENTS; segment++)
        program_segment(board, digit, segment, 1);
#endif
}


void terminate_sim(void)
{
  endwin();
  exit(0);
}

void _delay_ms(int ms)
{
  usleep(ms * 1000);
}

int xx = 0;

void program_segment(uint8_t board, uint8_t digit, uint8_t segment, uint8_t onoff)
{
  if (board < 0 || board >= NUM_BOARDS || digit < 0 || digit >= NUM_DIGITS || segment < 0 || segment >= 8)
    return;

  int x_origin = (NUM_DIGITS - digit - 1) * DIGIT_WIDTH;
  int y_origin = board * DIGIT_HEIGHT;
  int i;

  x_origin += segment_defs[segment].xoff;
  y_origin += segment_defs[segment].yoff;

  if (onoff)
    mvwprintw(mainwnd, y_origin, x_origin, segment_defs[segment].s);
  else {
    for (i = strlen(segment_defs[segment].s); i; i--)
      mvwprintw(mainwnd, y_origin, x_origin+i-1, " ");
  }
    
  wrefresh(mainwnd);
}

char scan_keyboard()
{
  char c = getch();

  if (c == 'q')
    terminate_sim();

  if (c >= 'a' && c < 'd')
    return c;

  if (c >= '0' && c <= '9')
    return c;

  if (c == '*' || c == '#')
    return c;

  return 0;
}


