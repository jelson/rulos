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
	
	schedule_us(50, (Activation*) qk);
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
	qk->func = (ActivationFunc) qk_update;

	qk->ifi = ifi;
	qk->fwd = fwd;
	qk->back = back;

#ifndef SIM
	qk->pin0 = pin0;
	qk->pin1 = pin1;
	gpio_make_input(PINUSE(*pin0));
	gpio_make_input(PINUSE(*pin1));
#endif //SIM

	schedule_us(1, (Activation*) qk);
}
