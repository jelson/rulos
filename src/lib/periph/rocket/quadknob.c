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

#include "quadknob.h"

#define XX	0	/* invalid transition */
// TODO this is 4 bytes of table, stored in 16 bytes.
static int8_t quad_state_machine[16] = {
	 0,	//0000
	+1,	//0001
	-1,	//0010
	XX,	//0011
	-1,	//0100
	 0,	//0101
	XX,	//0110
	+1,	//0111
	+1,	//1000
	XX,	//1001
	 0,	//1010
	-1,	//1011
	XX,	//1100
	-1,	//1101
	+1,	//1110
	 0	//1111
};

void qk_update(QuadKnob *qk)
{
	r_bool c0, c1;
#if SIM
	c0 = 0;
	c1 = 0;
#else
	c0 = reg_is_set(qk->pin0->pin, qk->pin0->bit);
	c1 = reg_is_set(qk->pin1->pin, qk->pin1->bit);
#endif
	uint8_t newState = (c1<<1) | c0;
	uint8_t transition = (qk->oldState << 2) | newState;
	int8_t delta = quad_state_machine[transition];
	qk->oldState = newState;

	if (delta==-1)
	{
		qk->ifi->func(qk->ifi, qk->back);
	}
	else if (delta==1)
	{
		qk->ifi->func(qk->ifi, qk->fwd);
	}
	
	schedule_us(50, (ActivationFuncPtr) qk_update, qk);
}

void init_quadknob(QuadKnob *qk,
	InputInjectorIfc *ifi,
#ifndef SIM
	IOPinDef *pin0,
	IOPinDef *pin1,
#endif //SIM
	char fwd,
	char back)
{
	qk->ifi = ifi;
	qk->fwd = fwd;
	qk->back = back;

#ifndef SIM
	qk->pin0 = pin0;
	qk->pin1 = pin1;
	gpio_make_input_enable_pullup(PINUSE(*pin0));
	gpio_make_input_enable_pullup(PINUSE(*pin1));
#endif //SIM

	schedule_us(1, (ActivationFuncPtr) qk_update, qk);
}
