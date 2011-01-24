/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "rocket.h"
#include "clock.h"
#include "util.h"
#include "network.h"
#include "sim.h"
#include "audio_driver.h"
#include "audio_server.h"
#include "audio_streamer.h"
#include "sdcard.h"
#include "serial_console.h"
#include "input_controller.h"

#include "graphic_lcd_12232.h"
#include "led_matrix_single.h"

#if !SIM
#include "hardware.h"
#endif // !SIM

//////////////////////////////////////////////////////////////////////////////

SerialConsole *g_console;

#define SYNCDEBUG()	syncdebug(0, 'T', __LINE__)
void syncdebug(uint8_t spaces, char f, uint16_t line)
{
	char buf[32], hexbuf[6];
	int i;
	for (i=0; i<spaces; i++)
	{
		buf[i] = ' ';
	}
	buf[i++] = f;
	buf[i++] = ':';
	buf[i++] = '\0';
	if (f >= 'a')	// lowercase -> hex value; so sue me
	{
		debug_itoha(hexbuf, line);
	}
	else
	{
		itoda(hexbuf, line);
	}
	strcat(buf, hexbuf);
	strcat(buf, "\n");
	serial_console_sync_send(g_console, buf, strlen(buf));
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	Activation act;
	uint8_t val;
} BlinkAct;

void _update_blink(Activation *act)
{
	BlinkAct *ba = (BlinkAct *) act;
	ba->val += 1;
#if 0
	gpio_set_or_clr(GPIO_D3, ((ba->val>>0)&0x1)==0);
	gpio_set_or_clr(GPIO_D4, ((ba->val>>4)&0x1)==0);
#endif // !SIM
	if ((ba->val & 0xf)==0) {
//		SYNCDEBUG();
	}
	schedule_us(100000, &ba->act);
}

void blink_init(BlinkAct *ba)
{
	ba->act.func = _update_blink;
	ba->val = 0;
#if 0
	gpio_make_output(GPIO_D3);
	gpio_make_output(GPIO_D4);
#endif // !SIM
	schedule_us(100000, &ba->act);
}

//////////////////////////////////////////////////////////////////////////////

struct s_flashcard;
typedef struct s_flashcard Flashcard;

typedef struct {
	InputInjectorIfc iii;
	Flashcard *fl;
} FlashcardInjector;

struct s_flashcard {
	Activation act;
	GLCD glcd;
	InputPollerAct ipoll;
	FlashcardInjector fi;
	uint8_t operand[2];
	uint8_t operator;
	uint8_t digit[2];
	uint8_t goal_value;
	r_bool displaying_result;
};

void _fl_new_problem(Flashcard *fl);
void _flashcard_update(Activation *act);
void _flashcard_do_input(InputInjectorIfc *iii, char key);
void _flashcard_draw(Flashcard *fl);

void flashcard_init(Flashcard *fl)
{
	fl->act.func = _flashcard_update;
	input_poller_init(&fl->ipoll, &fl->fi.iii);
	fl->fi.iii.func = _flashcard_do_input;
	fl->fi.fl = fl;
	_fl_new_problem(fl);
	glcd_init(&fl->glcd, &fl->act);
}

void _flashcard_update(Activation *act)
{
	Flashcard *fl = (Flashcard *) act;
	SYNCDEBUG();

	if (fl->displaying_result)
	{
		fl->displaying_result = false;
		_fl_new_problem(fl);
	}
	else
	{
		// that's an init completing.
	}
	_flashcard_draw(fl);
}

void _flashcard_itoa(Flashcard *fl, char **out, uint8_t v)
{
	if (v>=10)
	{
		uint8_t d = v/10;
		**out = d+'0';
		(*out)+=1;
		v -= (d*10);
	}
	**out = v+'0';
	(*out)+=1;
}

void _flashcard_paint(Flashcard *fl, char *s)
{
	char *p;
	int16_t width, dx;

	for (p=s, width=0; *p!=0 && *p!='\n'; p++)
	{
		width+=glcd_paint_char(&fl->glcd, (*p) & 0x7f, width, (*p)&0x80);
	}
	if (width>122)
	{
		dx = 122-width;
	}
	else
	{
		dx = 0;
	}

	serial_console_sync_send(g_console, s, strlen(s));
	serial_console_sync_send(g_console, "\n", 1);
	glcd_clear_framebuffer(&fl->glcd);

	for (p=s; *p!=0 && *p!='\n'; p++)
	{
		dx+=glcd_paint_char(&fl->glcd, (*p) & 0x7f, dx, (*p)&0x80);
	}
	glcd_draw_framebuffer(&fl->glcd);
}

void _flashcard_draw(Flashcard *fl)
{
	char string[10];
	char *p = string;
	_flashcard_itoa(fl, &p, fl->operand[0]);
	*(p++) = fl->operator;
	_flashcard_itoa(fl, &p, fl->operand[1]);
	*(p++) = 'e';
	*(p++) = fl->digit[0] | 0x80;
	*(p++) = fl->digit[1] | 0x80;
	*(p++) = '\0';
	_flashcard_paint(fl, string);
}

void _fl_new_problem(Flashcard *fl)
{
	deadbeef_srand(clock_time_us());
	uint8_t which_operator = (deadbeef_rand() & 0x400) > 0;

	syncdebug(2, 'w', which_operator);
	if (which_operator==0)
	{
		fl->operand[0] = deadbeef_rand() % 100;
		fl->operand[1] = deadbeef_rand() % (100-fl->operand[0]);
		fl->goal_value = fl->operand[0]+fl->operand[1];
		fl->operator = 'p';
	}
	else
	{
		fl->operand[0] = deadbeef_rand() % 10;
		fl->operand[1] = deadbeef_rand() % 10;
		fl->goal_value = fl->operand[0]*fl->operand[1];
		fl->operator = 't';
	}
	fl->digit[0] = ' ';
	fl->digit[1] = ' ';
	fl->displaying_result = false;
}

inline static uint8_t digit_value(char digit)
{
	if (digit>='0' && digit<='9')
	{
		return digit - '0';
	}
	return 0;
}

void _flashcard_enter(Flashcard *fl)
{
	uint8_t entered_value =
		digit_value(fl->digit[0])*10
		+ digit_value(fl->digit[1]);
	syncdebug(2, 'd', digit_value(fl->digit[1]));
	syncdebug(3, 'd', digit_value(fl->digit[0]));
	if (entered_value == fl->goal_value)
	{
		_flashcard_paint(fl, "W");
	}
	else
	{
		syncdebug(2, 'E', entered_value);
		syncdebug(2, 'G', fl->goal_value);
		_flashcard_paint(fl, "L");
	}
	fl->displaying_result = true;
	schedule_us(1000000, &fl->act);
}

void _flashcard_do_input(InputInjectorIfc *iii, char key)
{
	Flashcard *fl = ((FlashcardInjector *) iii)->fl;

	syncdebug(7, 'k', key);

	if (fl->displaying_result)
	{
		SYNCDEBUG();
		return;
	}

	switch (key)
	{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			fl->digit[0] = fl->digit[1];
			fl->digit[1] = key;
			_flashcard_draw(fl);
			break;
		case 'd':
			fl->digit[0] = ' ';
			fl->digit[1] = ' ';
			_flashcard_draw(fl);
			break;
		case 'c':
			_flashcard_enter(fl);
			//explode();
			break;
	}
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	Activation act;
	uint16_t col;
	uint8_t row;
} LEDWalk;

void ledwalk_update(Activation *act);

void ledwalk_init(LEDWalk *lw)
{
	lw->act.func = ledwalk_update;
	lw->col = 1;
	lw->row = 1;
	schedule_us(50000, &lw->act);
}

void ledwalk_update(Activation *act)
{
	LEDWalk *lw = (LEDWalk *)act;
	if (lw->col==0x8000)
	{
		if (lw->row==0x80)
		{
			lw->row = 1;
		}
		else
		{
			lw->row = lw->row << 1;
		}
		_lms_configure_row(lw->row);
		lw->col = 0x0001;
		_lms_configure_col(lw->col);
	}
	else
	{
		lw->col = lw->col << 1;
		_lms_configure_col(lw->col);
	}
	schedule_us(50000, &lw->act);
}

//////////////////////////////////////////////////////////////////////////////

#define LMS_OUTPUT_ENABLE_INV	GPIO_D6
#define LMS_DATA				GPIO_D5
#define LMS_SHIFT_CLOCK			GPIO_D4
#define LMS_LATCH_ENABLE_COLS	GPIO_D3
#define LMS_LATCH_ENABLE_ROWS	GPIO_D2

typedef struct {
	InputInjectorIfc iii;
	InputPollerAct ipoll;
	r_bool oe;
	r_bool data;
	r_bool clock;
	r_bool lec;
	r_bool ler;
} LEDPoke;

void _ledpoke_handler(InputInjectorIfc *iii, char key);

void ledpoke_init(LEDPoke *ledpoke)
{
	ledpoke->iii.func = _ledpoke_handler;
	input_poller_init(&ledpoke->ipoll, &ledpoke->iii);
}

void _ledpoke_handler(InputInjectorIfc *iii, char key)
{
#if !SIM
	LEDPoke *ledpoke = (LEDPoke *) iii;
	switch (key)
	{
		case '3':
			ledpoke->oe = !ledpoke->oe;
			gpio_set_or_clr(LMS_OUTPUT_ENABLE_INV, ledpoke->oe);
			break;
		case '4':
			ledpoke->data = !ledpoke->data;
			gpio_set_or_clr(LMS_DATA, ledpoke->data);
			break;
		case '5':
			ledpoke->clock = !ledpoke->clock;
			gpio_set_or_clr(LMS_SHIFT_CLOCK, ledpoke->clock);
			break;
		case '6':
			ledpoke->lec = !ledpoke->lec;
			gpio_set_or_clr(LMS_LATCH_ENABLE_COLS, ledpoke->lec);
			break;
		case '7':
			ledpoke->ler = !ledpoke->ler;
			gpio_set_or_clr(LMS_LATCH_ENABLE_ROWS, ledpoke->ler);
			break;
	}
#endif // !SIM
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	Activation act;
	SerialConsole console;
	Flashcard *fl;
	uint8_t rowdata;
	uint16_t coldata;
} Shell;

void shell_func(Activation *act);

void shell_init(Shell *shell, Flashcard *fl)
{
	shell->act.func = shell_func;
	shell->fl = fl;
	serial_console_init(&shell->console, &shell->act);
	g_console = &shell->console;
	SYNCDEBUG();
}

void shell_func(Activation *act)
{
	Shell *shell = (Shell *) act;
	char *line = shell->console.line;

#if !SIM
	if (strncmp(line, "key", 3)==0)
	{
		SYNCDEBUG();
		char k = line[4];
		(shell->fl->fi.iii.func)(&shell->fl->fi.iii, k);
	}
	else if (strncmp(line, "row", 3)==0)
	{
		SYNCDEBUG();
		shell->rowdata = atoi_hex(&line[4]);
		syncdebug(2, 'r', shell->rowdata);
		_lms_configure_row(shell->rowdata);
	}
	else if (strncmp(line, "col", 3)==0)
	{
		SYNCDEBUG();
		shell->coldata = atoi_hex(&line[4]);
		syncdebug(2, 'c', shell->coldata);
		_lms_configure_col(shell->coldata);
	}
#else
	line++;
#endif // !SIM
	serial_console_sync_send(&shell->console, "OK\n", 3);
}

//////////////////////////////////////////////////////////////////////////////

int main()
{
	util_init();
	hal_init(bc_audioboard);	// TODO need a "bc_custom"
	init_clock(1000, TIMER1);
	hal_init_keypad();

	Flashcard fl;
	
	Shell shell;
	shell_init(&shell, &fl);
		// syncdebug (uart) ready here.

	flashcard_init(&fl);

	BlinkAct ba;
	blink_init(&ba);

	LEDMatrixSingle lms;
	led_matrix_single_init(&lms);

/*
	LEDWalk lw;
	ledwalk_init(&lw);

	LEDPoke lp;
	ledpoke_init(&lp);
*/

	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase
	cpumon_main_loop();
	while (1) { }

	return 0;
}

