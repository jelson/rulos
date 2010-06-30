#include "rocket.h"


typedef struct {
	ActivationFunc f;
	char sendBuf[30];
	char recvBuf[30];
	TWIRecvSlot *trs;
} accelAct_t;


void readAccel(accelAct_t *aa)
{
	aa->sendBuf[0] = 0x2;
	hal_twi_send(0b1111000, aa->sendBuf, 1, NULL, NULL);

	schedule_us(1000000, (Activation *) aa);
}


int main()
{
	heap_init();
	util_init();
	hal_init(bc_audioboard);
	init_clock(10000, TIMER1);

	accelAct_t aa;
	aa.f = (ActivationFunc) readAccel;
	hal_twi_init(0, NULL);
	schedule_now((Activation *) &aa);

	CpumonAct cpumon;
	cpumon_init(&cpumon);
	cpumon_main_loop();
	assert(FALSE);


	return 0;
}
