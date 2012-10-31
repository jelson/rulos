#include "animate.h"

#ifndef SIM
#include "hardware.h"

#if 0
#define FRZ_LED0	GPIO_D0
#define FRZ_LED1	GPIO_D1
#define FRZ_LED2	GPIO_D2
#define FRZ_LED3	GPIO_D3
#define FRZ_LED4	GPIO_D4
#define FRZ_LED5	GPIO_D5
#define FRZ_LED6	GPIO_D6
#define FRZ_LED7	GPIO_D7
#else
#define FRZ_LED0	GPIO_D6
#define FRZ_LED1	GPIO_D7
#define FRZ_LED2	GPIO_D5
#define FRZ_LED3	GPIO_D2
#define FRZ_LED4	GPIO_D0
#define FRZ_LED5	GPIO_D3
#define FRZ_LED6	GPIO_D1
//#define FRZ_LED7	GPIO_D4
#define FRZ_LED7	GPIO_C3
#define FRZ_DBG		GPIO_D4
#endif

#endif // SIM

/*
PWM at 16ms (62Hz); six brightnesses [16, 8, 4, 2, 1, 0].
So let's measure animation frames at 16ms.
*/

#define END_SEQUENCE	(0)

typedef struct s_cell {
	uint16_t nframes;	// time in 16ms frames. END_SEQUENCE => end sequence.
	const char* brights;		// 8 chars.
} Cell;

Cell movie_idle[] = {
	{ 250, "00012345" },
	{ 250, "54321000" },
	{ END_SEQUENCE, NULL } };

Cell movie_zap[] = {
	{ 6, "50000000" },
	{ 6, "45000000" },
	{ 6, "34500000" },
	{ 6, "23450000" },
	{ 6, "12345000" },
	{ 6, "01234500" },
	{ 6, "00123450" },
	{ 6, "00012345" },
	{ 6, "00001234" },
	{ 6, "00000123" },
	{ 6, "00000012" },
	{ 6, "00000001" },
	{ END_SEQUENCE, NULL } };

Cell movie_diddle[] = {
	{ 1000, "50505050" },
	{ END_SEQUENCE, NULL } };

static uint8_t _pwm_phases[16] = {
	0x3e, 0x3c, 0x38, 0x38, 0x30, 0x30, 0x30, 0x30,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20 };

static inline uint8_t _pwm(uint8_t phase, char bright)
{
	return ((_pwm_phases[phase] >> bright)&1);
}

static inline void _set_leds(const char *brights, uint8_t phase)
{
#ifndef SIM
	gpio_set_or_clr(FRZ_LED0, _pwm(phase, brights[0]-'0'));
	gpio_set_or_clr(FRZ_LED1, _pwm(phase, brights[1]-'0'));
	gpio_set_or_clr(FRZ_LED2, _pwm(phase, brights[2]-'0'));
	gpio_set_or_clr(FRZ_LED3, _pwm(phase, brights[3]-'0'));
	gpio_set_or_clr(FRZ_LED4, _pwm(phase, brights[4]-'0'));
	gpio_set_or_clr(FRZ_LED5, _pwm(phase, brights[5]-'0'));
	gpio_set_or_clr(FRZ_LED6, _pwm(phase, brights[6]-'0'));
	gpio_set_or_clr(FRZ_LED7, _pwm(phase, brights[7]-'0'));
#endif //SIM
}

void _animate_act(void* v_an)
{
	Animate* an = (Animate*) v_an;
	schedule_us(1000, _animate_act, an);
		// hope we get done in 8000 cycles, before next scheduling!
		// schedule early for more-consistent timing (?)

	an->phase = ((an->phase+1)&0x0f);

	_set_leds(an->cell->brights, an->phase);
	an->delay--;

	if (an->delay == 0)
	{
		an->cell++;
		if (an->cell->nframes == END_SEQUENCE)
		{
			animate_play(an, Idle);
			an->cell = movie_idle;
		}
		an->delay = an->cell->nframes;
	}
}

void animate_init(Animate* an)
{
#ifndef SIM
	gpio_make_output(FRZ_LED0);
	gpio_make_output(FRZ_LED1);
	gpio_make_output(FRZ_LED2);
	gpio_make_output(FRZ_LED3);
	gpio_make_output(FRZ_LED4);
	gpio_make_output(FRZ_LED5);
	gpio_make_output(FRZ_LED6);
	gpio_make_output(FRZ_LED7);
	gpio_make_output(FRZ_DBG);

	gpio_clr(FRZ_LED0);
	gpio_clr(FRZ_LED1);
	gpio_clr(FRZ_LED2);
	gpio_clr(FRZ_LED3);
	gpio_clr(FRZ_LED4);
	gpio_clr(FRZ_LED5);
	gpio_clr(FRZ_LED6);
	gpio_set(FRZ_LED7);
	gpio_set(FRZ_DBG);
#endif // SIM

	an->phase = 0;
	an->delay = 0;
	an->cell = movie_idle;

//	_set_leds("00543210", 0);

	schedule_us(1, _animate_act, an);
}

void animate_play(Animate* an, Movie movie)
{
	switch (movie)
	{
	case Idle:
		an->cell = movie_idle;
		break;
	case Zap:
		an->cell = movie_zap;
		break;
	case Diddle:
		an->cell = movie_diddle;
		break;
	}
	an->delay = an->cell->nframes;
}
