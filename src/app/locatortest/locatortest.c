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

	schedule_us(50000, (Activation *) aa);
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

static inline int16_t convert_accel(uint8_t msb, uint8_t lsb)
{
	int16_t mag = ((int16_t) msb & 0b01111111) << 2 | (lsb >> 6);
	
	if (msb & 0b10000000)
		mag = mag-512;

	return mag;
}


void completeAccelRead(TWIRecvSlot *trs, uint8_t len)
{
	accelAct_t *aa = (accelAct_t *) trs->user_data;

	// send the data out of serial
	uint16_t x = convert_accel(trs->data[1], trs->data[0]);
	uint16_t y = convert_accel(trs->data[3], trs->data[2]);
	uint16_t z = convert_accel(trs->data[5], trs->data[4]);
	sprintf(aa->sendBuf, "a;x=%5d;y=%5d;z=%5d\r\n", x, y, z);

	uart_send(RULOS_UART0, aa->sendBuf, strlen(aa->sendBuf), NULL, NULL);
}



int main()
{
	heap_init();
	util_init();
	hal_init(bc_audioboard);
	init_clock(10000, TIMER1);

	hal_twi_init(0, NULL);
	uart_init(RULOS_UART0, 12);

	accelAct_t aa;
	aa.f = (ActivationFunc) prepareAccelRead;
	schedule_now((Activation *) &aa);

	CpumonAct cpumon;
	cpumon_init(&cpumon);
	cpumon_main_loop();
	assert(FALSE);


	return 0;
}
