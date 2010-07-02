#include "rocket.h"


typedef struct {
	ActivationFunc f;
	char sendBuf[30];
	char recvBuf[30];
	TWIRecvSlot *trs;
} accelAct_t;

#define ACCEL_ADDR 0b0111000


void doAccelRead(void *user_data);
void completeAccelRead(TWIRecvSlot *trs, uint8_t len);

void prepareAccelRead(accelAct_t *aa)
{
	aa->sendBuf[0] = 0x2;
	hal_twi_send(ACCEL_ADDR, aa->sendBuf, 1, doAccelRead, aa);

	schedule_us(1000000, (Activation *) aa);
}

void doAccelRead(void *user_data)
{
	accelAct_t *aa = (accelAct_t *) user_data;
	aa->trs = (TWIRecvSlot *) aa->recvBuf;
	aa->trs->func = completeAccelRead;
	aa->trs->capacity = 6;
	aa->trs->user_data = aa;
	hal_twi_read(ACCEL_ADDR, aa->trs);
}

void completeAccelRead(TWIRecvSlot *trs, uint8_t len)
{
	// send the data out of serial
	uint16_t x = ((uint16_t) trs->data[1] << 8) | (trs->data[0] >> 6);
	uint16_t y = ((uint16_t) trs->data[3] << 8) | (trs->data[2] >> 6);
	uint16_t z = ((uint16_t) trs->data[5] << 8) | (trs->data[4] >> 6);

	char buf[30];

	sprintf(buf, "a;x=%d;y=%d;z=%d\n", x, y, z);
	uart_send(RULOS_UART0, buf, strlen(buf), NULL, NULL);
}



int main()
{
	heap_init();
	util_init();
	hal_init(bc_audioboard);
	init_clock(50000, TIMER1);

	hal_twi_init(0, NULL);

	accelAct_t aa;
	aa.f = (ActivationFunc) prepareAccelRead;
	schedule_now((Activation *) &aa);

	CpumonAct cpumon;
	cpumon_init(&cpumon);
	cpumon_main_loop();
	assert(FALSE);


	return 0;
}
