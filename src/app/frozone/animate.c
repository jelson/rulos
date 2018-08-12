#include "animate.h"

#include <stdlib.h>
#include "core/clock.h"

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

#define FRZ_LED7	GPIO_C3
#define FRZ_DBG		GPIO_D4
#else
#define FRZ_LED0	GPIO_D6
#define FRZ_LED1	GPIO_D7
#define FRZ_LED2	GPIO_D5
#define FRZ_LED3	GPIO_D2
#define FRZ_LED4	GPIO_D0
#define FRZ_LED5	GPIO_D3
#define FRZ_LED6	GPIO_D1
#define FRZ_LED7	GPIO_D4
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

#define QUEISCE	16000
Cell movie_idle[] = {
	{QUEISCE, "00000000" },
	{   20, "10100010" },
	{   50, "00000000" },
	{   20, "00101010" },
	{QUEISCE, "00000000" },
	{   20, "01010101" },
	{   50, "00000000" },
	{   20, "05010003" },
	{   50, "00000000" },
	{   20, "00050200" },
	{QUEISCE, "00000000" },
	{   20, "00011000" },
	{   50, "00000000" },
	{   20, "01003010" },
	{   50, "00000000" },
	{   20, "01000010" },
	{ END_SEQUENCE, NULL } };

#define ZAP_RATE	15
Cell movie_zap[] = {
	{ ZAP_RATE, "00000001" },
	{ ZAP_RATE, "00000012" },
	{ ZAP_RATE, "00000123" },
	{ ZAP_RATE, "00001234" },
	{ ZAP_RATE, "00012345" },
	{ ZAP_RATE, "00123450" },
	{ ZAP_RATE, "01234500" },
	{ ZAP_RATE, "12345000" },
	{ ZAP_RATE, "23450000" },
	{ ZAP_RATE, "34500000" },
	{ ZAP_RATE, "45000000" },
	{ ZAP_RATE, "50000000" },
	{ END_SEQUENCE, NULL } };

#define THROB_RATE	50
Cell movie_diddle[] = {
	{ THROB_RATE, "00011000" },
	{ THROB_RATE, "00122100" },
	{ THROB_RATE, "12344321" },
	{ THROB_RATE, "23455432" },
	{ THROB_RATE, "34500534" },
	{ THROB_RATE, "34500534" },
	{ THROB_RATE, "23455432" },
	{ THROB_RATE, "12344321" },
	{ THROB_RATE, "01233210" },
	{ THROB_RATE, "00122100" },
	{ THROB_RATE, "00011000" },
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
#ifdef FRZ_DBG
		static uint8_t foo;
		foo++;
		gpio_set_or_clr(FRZ_DBG, foo&1);
#endif // FRZ_DBG
	
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
	gpio_clr(FRZ_LED0);
	gpio_clr(FRZ_LED1);
	gpio_clr(FRZ_LED2);
	gpio_clr(FRZ_LED3);
	gpio_clr(FRZ_LED4);
	gpio_clr(FRZ_LED5);
	gpio_clr(FRZ_LED6);
	gpio_set(FRZ_LED7);
#ifdef FRZ_DBG
	gpio_make_output(FRZ_DBG);

	gpio_set(FRZ_DBG);
#endif // FRZ_DBG
#endif // SIM

	an->phase = 0;
	an->delay = 0;
	animate_play(an, Idle);

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
