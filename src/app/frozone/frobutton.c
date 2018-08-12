#include "frobutton.h"
#include "core/clock.h"

#ifndef SIM

#include "hardware.h"

#define FRZ_SW0		GPIO_C4
#define FRZ_SW1		GPIO_C5

#endif // SIM

static void _frobutton_act(void* v_fb)
{
	Frobutton* fb = (Frobutton*) v_fb;

#ifndef SIM
	if (!gpio_is_set(FRZ_SW0))
	{
		animate_play(fb->animate, Zap);
	}
	else if (!gpio_is_set(FRZ_SW1))
	{
		animate_play(fb->animate, Diddle);
	}
#endif // SIM

	schedule_us(50000, _frobutton_act, fb);
}

void frobutton_init(Frobutton* fb, Animate* an)
{
#ifndef SIM
	gpio_make_input_enable_pullup(FRZ_SW0);
	gpio_make_input_enable_pullup(FRZ_SW1);
#endif // SIM

	fb->animate = an;
	schedule_us(1, _frobutton_act, fb);
}
