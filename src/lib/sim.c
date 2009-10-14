#define DIGIT_WIDTH 5
#define DIGIT_HEIGHT 4

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <signal.h>
#include <sched.h>

#include "rocket.h"
#include "util.h"
#include "display_controller.h"

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

  
sigset_t alrm_set;

void hal_init()
{
  int board, digit;
  mainwnd = initscr();
  noecho();
  cbreak();
  nodelay(mainwnd, TRUE);
  refresh(); // 1)
  wrefresh(mainwnd);
  curs_set(0);

  SSBitmap value = ascii_to_bitmap('8') | SSB_DECIMAL;
  for (board = 0; board < NUM_BOARDS; board++)
  {
    for (digit = 0; digit < NUM_DIGITS; digit++)
	{
		program_cell(board, digit, value);
	}
  }

	sigemptyset(&alrm_set);
	sigaddset(&alrm_set, SIGALRM);
}

#if 0 // deprecated
void sim_run()
{
  while (1)
  {
    sleep(1);
	scheduler_ru
  }
}
#endif

void terminate_sim(void)
{
  endwin();
  exit(0);
}

#if 0
void delay_ms(int ms)
{
	// usleep() can be interrupted by signals, so I retry.
	// this function should go away, being replaced by a non-simulated
	// clock based on (simulated) real time.
	struct timeb start;
	ftime(&start);

	int elapsed_ms;
	while (1)
	{
		struct timeb now;
		ftime(&now);
		elapsed_ms = (now.time - start.time)*1000 + (now.millitm - start.millitm);
		if (elapsed_ms >= ms)
		{
			break;
		}
		usleep((ms - elapsed_ms) * 1000);
	}
}
#endif

void hal_program_segment(uint8_t board, uint8_t digit, uint8_t segment, uint8_t onoff)
{
  if (board < 0 || board >= NUM_BOARDS || digit < 0 || digit >= NUM_DIGITS || segment < 0 || segment >= 8)
    return;

  int x_origin = (digit) * DIGIT_WIDTH;
  int y_origin = board * DIGIT_HEIGHT;
  int i;

  x_origin += segment_defs[segment].xoff;
  y_origin += segment_defs[segment].yoff;

  if (onoff)
  {
    mvwprintw(mainwnd, y_origin, x_origin, segment_defs[segment].s);
  }
  else
  {
    for (i = strlen(segment_defs[segment].s); i; i--)
      mvwprintw(mainwnd, y_origin, x_origin+i-1, " ");
  }
    
  wrefresh(mainwnd);
}

char hal_read_keybuf()
{
  char c = getch();

  if (c == 'q')
    terminate_sim();

  if (c >= 'a' && c <= 'd')
    return c;
  if (c == '\t') return 'a';
  if (c == '\n') return 'c';
  if (c == 27) return 'd';

  if (c >= '0' && c <= '9')
    return c;

  if (c == '*' || c == 's' || c == '.')
    return 's';

  if (c == 'p' || c == '#')
    return 'p';

  return 0;
}


void hal_start_clock_us(uint32_t us, Handler handler)
{
	struct itimerval ivalue, ovalue;
	ivalue.it_interval.tv_sec = us/1000000;
	ivalue.it_interval.tv_usec = (us%1000000);
	ivalue.it_value = ivalue.it_interval;
	setitimer(ITIMER_REAL, &ivalue, &ovalue);

	signal(SIGALRM, handler);
}

void hal_start_atomic()
{
	sigprocmask(SIG_BLOCK, &alrm_set, NULL);
}

void hal_end_atomic()
{
	sigprocmask(SIG_UNBLOCK, &alrm_set, NULL);
}

void hal_idle()
{
	// turns out 'man sleep' says sleep & sigalrm don't mix. yield is what we want.
	// No, sched_yield doesn't wait ANY time. libc suggests select()
	static struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 100;	// .1 ms
	select(0, NULL, NULL, NULL, &tv);
}
