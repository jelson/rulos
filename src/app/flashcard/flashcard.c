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
#include "eeprom.h"

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
#if 0
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
#endif
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

#include "starbitmaps.ch"
uint8_t lma_question[] = { 0x3c, 0x42, 0x02, 0x04, 0x08, 0x08, 0x00, 0x08 };
uint8_t lma_x[] = { 0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81 };
uint8_t lma_disc[] = { 0x3c, 0x7e, 0xff, 0xff, 0xff, 0xff, 0x7e, 0x3c };

typedef enum {
	lma_black,
	lma_red_x,
	lma_green_disc,
	lma_blinky_question_over_score_bitmap,
	lma_spinning_star
} LMAMode;

typedef struct {
	Activation act;
	LEDMatrixSingle lms;
	LMSBitmap score_bitmap;
	uint8_t star_index;
	LMAMode mode;
} LMAnimation;

void _lma_update(Activation *act);

void lma_init(LMAnimation *lma)
{
	lma->act.func = _lma_update;
	lma->star_index = 0;
	memset(&lma->score_bitmap, 0, sizeof(lma->score_bitmap));
	lma->mode = lma_black;
	led_matrix_single_init(&lma->lms, TIMER2);
	schedule_us(1000, &lma->act);
}

void memor(uint8_t *dst, uint8_t *src, uint16_t len)
{
	for (; len>0; len--, dst++, src++)
	{
		*dst |= *src;
	}
}

void _lma_update(Activation *act)
{
	LMAnimation *lma = (LMAnimation *) act;
	LEDMatrixSingle *lms = &lma->lms;

	switch (lma->mode)
	{
	case lma_black:
		memset(&lms->bitmap, 0, sizeof(lms->bitmap));
		break;
	case lma_red_x:
		memset(lms->bitmap.green, 0, sizeof(lms->bitmap.green));
		memcpy(lms->bitmap.red, lma_x, sizeof(lms->bitmap.red));
		break;
	case lma_green_disc:
		memcpy(lms->bitmap.green, lma_disc, sizeof(lms->bitmap.green));
		memset(lms->bitmap.red, 0, sizeof(lms->bitmap.red));
		break;
	case lma_blinky_question_over_score_bitmap:
		memcpy(&lms->bitmap, &lma->score_bitmap, sizeof(lms->bitmap));
		if ((clock_time_us() >> 19) & 1)
		{
			memor(lms->bitmap.red, lma_question, sizeof(lms->bitmap.red));
			memor(lms->bitmap.green, lma_question, sizeof(lms->bitmap.green));
		}
		break;
	case lma_spinning_star:
		lma->star_index += 1;
		if (lma->star_index >= (sizeof(starbitmap)/sizeof(starbitmap[0])))
		{
			lma->star_index = 0;
		}
		memcpy(lms->bitmap.red, starbitmap[lma->star_index], sizeof(lms->bitmap.red));
		memcpy(lms->bitmap.green, starbitmap[lma->star_index], sizeof(lms->bitmap.green));
		break;
	}

	schedule_us(100000, &lma->act);
}

//////////////////////////////////////////////////////////////////////////////

typedef struct s_problem_set ProblemSet;
typedef void (select_f)(ProblemSet *ps, uint8_t *operands, uint8_t *problem_reference);
typedef r_bool (check_f)(ProblemSet *ps, uint8_t problem_reference, uint8_t answer);

struct s_problem_set {
	select_f *select;
	check_f *check;
	char operator;
	uint8_t num_problems;
	uint8_t num_problems_left;
	uint8_t bits[0];
};

void _problem_set_init(ProblemSet *ps, select_f *select, check_f *check, char operator, uint8_t num_problems, r_bool storage)
{
	ps->select = select;
	ps->check = check;
	ps->operator = operator;
	ps->num_problems = num_problems;
	ps->num_problems_left = num_problems;
	// NB magic assumption: caller allocates num_problems>>3 bytes of
	// storage immediately following this header.
	if (storage)
	{
		memset(ps->bits, 0, (num_problems+7)>>3);
	}
}

uint8_t problem_select(ProblemSet *ps)
{
	//SYNCDEBUG();
	deadbeef_srand(clock_time_us());
	uint8_t which_problem = deadbeef_rand() % ps->num_problems_left;
	uint8_t idx;

	//SYNCDEBUG();
	for (idx=0; idx<ps->num_problems; idx++)
	{
		//syncdebug(2, 'i', idx);
		r_bool occupied = ((ps->bits[idx>>3]&(1<<(7-(idx&7)))) != 0);
		//syncdebug(2, 'o', idx);
		//syncdebug(2, 'w', which_problem);
		if (!occupied)
		{
			if (which_problem==0)
			{
				break;
			}
			which_problem--;
		}
	}
	//SYNCDEBUG();
	assert(idx < ps->num_problems);
	//syncdebug(2, 'i', idx);
	//syncdebug(2, 'a', idx>>3);
	//syncdebug(2, 'b', ps->bits[idx>>3]);
	//syncdebug(2, 'm', (1<<(7-(idx&7))));
	assert(!((ps->bits[idx>>3]&(1<<(7-(idx&7)))) != 0));

	//SYNCDEBUG();
	return idx;
}

typedef struct {
	ProblemSet ps;
	uint8_t bits[13];
} TimesProblemSet;

r_bool _times_check(ProblemSet *ps, uint8_t problem_reference, uint8_t answer);
void _times_select(ProblemSet *ps, uint8_t *operands, uint8_t *problem_reference);

void times_problem_set_init(TimesProblemSet *tps)
{
	_problem_set_init(&tps->ps, &_times_select, &_times_check, 't', 100, true);
}

void _times_select(ProblemSet *ps, uint8_t *operands, uint8_t *problem_reference)
{
	//SYNCDEBUG();
	TimesProblemSet *tps = (TimesProblemSet *) ps;
	//SYNCDEBUG();
	uint8_t problem = problem_select(&tps->ps);
	//SYNCDEBUG();
	operands[0] = problem / 10;
	operands[1] = problem - (operands[0]*10);
	*problem_reference = problem;
	//SYNCDEBUG();
}

r_bool _times_check(ProblemSet *ps, uint8_t problem_reference, uint8_t answer)
{
	//SYNCDEBUG();
	uint8_t op0 = problem_reference / 10;
	uint8_t op1 = problem_reference - (op0 * 10);

	r_bool result;
	if (op0*op1 != answer)
	{
		result = false;
	}
	else
	{
		uint8_t idx = problem_reference;
		// TimesProblemSet *tps = (TimesProblemSet *) ps;
		ps->bits[idx>>3] |= (1<<(7-(idx&7)));
		assert((ps->bits[idx>>3]&(1<<(7-(idx&7)))) != 0);

		ps->num_problems_left -= 1;
		result = true;
	}
	//SYNCDEBUG();
	return result;
}

// There are ~100*50 = 5000 sum problems with 2-digit answers. Hmm. How should
// we reduce the space? No point, I guess; just select a random set of problems.

typedef struct {
	ProblemSet ps;
} SumProblemSet;

r_bool _sum_check(ProblemSet *ps, uint8_t problem_reference, uint8_t answer);
void _sum_select(ProblemSet *ps, uint8_t *operands, uint8_t *problem_reference);

void sum_problem_set_init(SumProblemSet *sps)
{
	_problem_set_init(&sps->ps, &_sum_select, &_sum_check, 'p', 100, false);
}

void _sum_select(ProblemSet *ps, uint8_t *operands, uint8_t *problem_reference)
{
	//SYNCDEBUG();
	//SumProblemSet *sps = (SumProblemSet *) ps;
	deadbeef_srand(clock_time_us());
	operands[0] = deadbeef_rand() % 100;
	operands[1] = deadbeef_rand() % (100-operands[0]);
	*problem_reference = operands[0] + operands[1];
	//SYNCDEBUG();
}

r_bool _sum_check(ProblemSet *ps, uint8_t problem_reference, uint8_t answer)
{
	//SYNCDEBUG();
	r_bool result;
	if (answer!=problem_reference)
	{
		result = false;
	}
	else
	{
		ps->num_problems_left -= 1;
		result = true;
	}
	//SYNCDEBUG();
	return result;
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	r_bool game_over;
	uint8_t num_incorrect;
	SumProblemSet sps;
	TimesProblemSet tps;
} GameState;

void game_state_init(GameState *game)
{
	game->game_over = false;
	game->num_incorrect = 0;
	sum_problem_set_init(&game->sps);
	times_problem_set_init(&game->tps);
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
	uint8_t digit[2];
	uint8_t problem_reference;
	r_bool displaying_result;
	ProblemSet *curProblem;
	LMAnimation lma;
	r_bool asking_reset;
	uint8_t reset_answer;

	GameState game;
};

void _fl_new_problem(Flashcard *fl);
void _flashcard_startup(Activation *act);
void _flashcard_update(Activation *act);
void _flashcard_do_input(InputInjectorIfc *iii, char key);
void _flashcard_draw(Flashcard *fl);
uint8_t _fl_num_problems_left(Flashcard *fl);
void _flashcard_paint(Flashcard *fl, char *s);

void flashcard_init(Flashcard *fl)
{
//SYNCDEBUG();
	input_poller_init(&fl->ipoll, &fl->fi.iii);
	fl->fi.iii.func = _flashcard_do_input;
	fl->fi.fl = fl;
//SYNCDEBUG();
	lma_init(&fl->lma);
	fl->asking_reset = false;
	fl->reset_answer = 0;

	fl->act.func = _flashcard_startup;
	glcd_init(&fl->glcd, &fl->act);
//SYNCDEBUG();
}

void _flashcard_startup(Activation *act)
{
	Flashcard *fl = (Flashcard *) act;

	r_bool valid;
	if (fl->asking_reset)
	{
		SYNCDEBUG();
		// we got awoken by input loop.
		if (fl->reset_answer == 'a')
		{
			// that is just the initial request getting counted. Wait.
			return;
		}

		fl->asking_reset = false;
		if (fl->reset_answer == 'c')
		{
			syncdebug(2, 'k', fl->reset_answer);
			SYNCDEBUG();
			valid = false;
		}
		else
		{
			SYNCDEBUG();
			valid = true;
		}
	}
	else
	{
		SYNCDEBUG();
		// we've just started up, and the LCD is ready.
		// Read the eeprom.
		valid = eeprom_read((uint8_t*) &fl->game, sizeof(fl->game));
		if (valid)
		{
			SYNCDEBUG();
			if (hal_scan_keypad()=='a')
			{
				SYNCDEBUG();
				// we found valid data; stop and
				// ask the user if he wishes to clear it.
				// We'll reenter up at "if (fl->asking_reset)".
				fl->displaying_result = false;
				_flashcard_paint(fl, "R");
				fl->asking_reset = true;
				fl->reset_answer = 0;
				return;
			}
		}
	}

	SYNCDEBUG();
	if (!valid)
	{
		SYNCDEBUG();
		game_state_init(&fl->game);
	}

	// Okay, we have a valid state, one way (eeprom) or another (initting it).
	// reconfigure into run mode.
	fl->act.func = _flashcard_update;
	schedule_us(1, &fl->act);
}

void _flashcard_draw_score_bitmap(Flashcard *fl)
{
	uint8_t total = fl->game.tps.ps.num_problems + fl->game.sps.ps.num_problems;
	uint8_t unsolved = _fl_num_problems_left(fl);
	uint8_t correct = total - unsolved;
	syncdebug(3, 't', total);
	syncdebug(3, 'u', unsolved);
	syncdebug(3, 'c', correct);
	syncdebug(3, 'p', (((uint16_t)correct)*48));
	uint8_t lit_greens = (((uint16_t)correct)*48)/total;
	uint8_t lit_reds = fl->game.num_incorrect;
	syncdebug(3, 'g', lit_greens);
	syncdebug(3, 'r', lit_reds);

	memset(&fl->lma.score_bitmap, 0, sizeof(fl->lma.score_bitmap));

	int8_t col;
	int8_t row;

	SYNCDEBUG();
	for (col=7; col>=0 && lit_reds>0; col--)
	{
		for (row=7; row>=0 && lit_reds>0; row--, lit_reds--)
		{
			fl->lma.score_bitmap.red[row] |= (1<<(7-col));
			syncdebug(0, 'r', row);
			syncdebug(0, 'c', col);
		}
	}

	// greens overwrite reds, so that even if you have a lot
	// of lose, your successes never get lost.
	SYNCDEBUG();
	for (col=0; col<8 && lit_greens>0; col++)
	{
		for (row=7; row>=0 && lit_greens>0; row--, lit_greens--)
		{
			fl->lma.score_bitmap.green[row] |= (1<<(7-col));
			syncdebug(0, 'r', row);
			syncdebug(0, 'c', col);
		}
	}
}

void _flashcard_update(Activation *act)
{
	Flashcard *fl = (Flashcard *) act;
	SYNCDEBUG();

	fl->displaying_result = false;
	_fl_new_problem(fl);

	if (!fl->game.game_over)
	{
		_flashcard_draw_score_bitmap(fl);
		fl->lma.mode = lma_blinky_question_over_score_bitmap;
		_flashcard_draw(fl);
	}
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

	lms_enable(&fl->lma.lms, false);
	SYNCDEBUG();
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
	lms_enable(&fl->lma.lms, true);
}

void _flashcard_draw(Flashcard *fl)
{
	SYNCDEBUG();
	char string[10];
	char *p = string;
	_flashcard_itoa(fl, &p, fl->operand[0]);
	*(p++) = fl->curProblem->operator;
	_flashcard_itoa(fl, &p, fl->operand[1]);
	*(p++) = 'e';
	*(p++) = fl->digit[0] | 0x80;
	*(p++) = fl->digit[1] | 0x80;
	*(p++) = '\0';
	_flashcard_paint(fl, string);
}

uint8_t _fl_num_problems_left(Flashcard *fl)
{
	return fl->game.sps.ps.num_problems_left + fl->game.tps.ps.num_problems_left;
}

void _fl_new_problem(Flashcard *fl)
{
	fl->displaying_result = false;

	SYNCDEBUG();
	r_bool sum_empty = (fl->game.sps.ps.num_problems_left==0);
	r_bool times_empty = (fl->game.tps.ps.num_problems_left==0);
	if (sum_empty && times_empty)
	{
		SYNCDEBUG();
		// Display victory and spinning star
		_flashcard_paint(fl, "V");
		fl->lma.mode = lma_spinning_star;
		// lock the keypad so we stay in this mode
		fl->game.game_over = true;
	}
	else if (sum_empty)
	{
		//SYNCDEBUG();
		fl->curProblem = &fl->game.tps.ps;
	}
	else if (times_empty)
	{
		//SYNCDEBUG();
		fl->curProblem = &fl->game.sps.ps;
	}
	else
	{
		//SYNCDEBUG();
		// choose one at random
		deadbeef_srand(clock_time_us());
		uint8_t which_operator = (deadbeef_rand() & 0x400) > 0;
		if (which_operator==0)
		{
			fl->curProblem = &fl->game.sps.ps;
		}
		else
		{
			fl->curProblem = &fl->game.tps.ps;
		}
	}

	SYNCDEBUG();
	(fl->curProblem->select)(fl->curProblem, fl->operand, &fl->problem_reference);

	fl->digit[0] = ' ';
	fl->digit[1] = ' ';
	//SYNCDEBUG();
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
	syncdebug(3, 'l', _fl_num_problems_left(fl));
	r_bool correct = (fl->curProblem->check)(fl->curProblem, fl->problem_reference, entered_value);
	if (correct)
	{
		_flashcard_paint(fl, "W");
		fl->lma.mode = lma_green_disc;
	}
	else
	{
		fl->game.num_incorrect += 1;
		_flashcard_paint(fl, "L");
		fl->lma.mode = lma_red_x;
	}
	eeprom_write((uint8_t*) &fl->game, sizeof(fl->game));
	fl->displaying_result = true;
//	SYNCDEBUG();
//	syncdebug(2, 'r', eeprom_read((uint8_t*) &fl->game, sizeof(fl->game)));
	schedule_us(1000000, &fl->act);
}

void _flashcard_do_input(InputInjectorIfc *iii, char key)
{
	Flashcard *fl = ((FlashcardInjector *) iii)->fl;

	syncdebug(7, 'k', key);

	if (fl->asking_reset)
	{
		fl->reset_answer = key;
		schedule_us(1, &fl->act);
		return;
	}

	if (fl->displaying_result || fl->game.game_over)
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
#if 0
		case 'b':
			// debug cheat code
			fl->game.tps.ps.num_problems_left = 1;
			fl->game.sps.ps.num_problems_left = 1;
			break;
#endif
		case 'c':
			_flashcard_enter(fl);
			//explode();
			break;
	}
}

//////////////////////////////////////////////////////////////////////////////

#if 0
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

#endif
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
	else if (strcmp(line, "t1\n")==0)
	{
		SYNCDEBUG();
//		_lms_configure_col(0xf00f);
		_lms_configure_row(0x40);
	}
	else if (strcmp(line, "t2\n")==0)
	{
		SYNCDEBUG();
//		_lms_configure_col(0xf00f);
		_lms_configure_row(0x80);
	}
	else if (strncmp(line, "pwm", 3)==0)
	{
		SYNCDEBUG();
//		lms.pwm_enable = atoi_hex(&line[4]);
//		void _lms_handler(void *arg);
//		_lms_handler(&lms);
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
