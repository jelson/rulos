#include "rocket.h"


typedef struct {
	ActivationFunc f;
	char TWIsendBuf[20];
	char TWIrecvBuf[5];
	char UARTsendBuf[30];
	TWIRecvSlot *trs;
} accelAct_t;

#define ACCEL_ADDR 0b0111000


void doAccelRead(void *user_data);
void completeAccelRead(TWIRecvSlot *trs, uint8_t len);

void prepareAccelRead(accelAct_t *aa)
{
	aa->TWIsendBuf[0] = 0x2;
	hal_twi_send(ACCEL_ADDR, aa->TWIsendBuf, 1, doAccelRead, aa);

	schedule_us(50000, (Activation *) aa);
}

void doAccelRead(void *user_data)
{
	accelAct_t *aa = (accelAct_t *) user_data;
	aa->trs = (TWIRecvSlot *) aa->TWIrecvBuf;
	aa->trs->func = completeAccelRead;
	aa->trs->capacity = 6;
	aa->trs->user_data = aa;
	hal_twi_read(ACCEL_ADDR, aa->trs);
}

static inline int16_t convert_accel(char lsb, char msb)
{
	int16_t mag = ((((int16_t) msb) & 0b01111111) << 2) | ((lsb & 0b11000000) >> 6);
	
	if (msb & 0b10000000)
		mag = mag-512;

	return mag;
}


void completeAccelRead(TWIRecvSlot *trs, uint8_t len)
{
	accelAct_t *aa = (accelAct_t *) trs->user_data;

	// send the data out of serial
	int16_t x = convert_accel(trs->data[0], trs->data[1]);
	int16_t y = convert_accel(trs->data[2], trs->data[3]);
	int16_t z = convert_accel(trs->data[4], trs->data[5]);
	snprintf(aa->UARTsendBuf, sizeof(aa->UARTsendBuf)-1, "a;x=%5d;y=%5d;z=%5d\r\n", x, y, z);

	uart_send(RULOS_UART0, aa->UARTsendBuf, strlen(aa->UARTsendBuf), NULL, NULL);
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
