#include "rocket.h"

struct accelAct;
typedef struct accelAct accelAct_t;

typedef void (*AccelSendCompleteFunc)(accelAct_t *aa);
typedef void (*AccelRecvCompleteFunc)(accelAct_t *aa, char *data, int len);


struct accelAct {
	ActivationFunc f;
	char TWIsendBuf[20];
	char TWIrecvBuf[5];
	char UARTsendBuf[35];
	TWIRecvSlot *trs;
	AccelSendCompleteFunc sendFunc;
	AccelRecvCompleteFunc recvFunc;
	uint8_t readLen;
};

#define ACCEL_ADDR 0b0111000

#define SAMPLING_RATE_BITS 0b001 // 50 hz
#define RANGE_BITS 0b01 // +/- 4g


/****************************************************/


/*** sending a packet to the accelerometer ***/
void _accelSendComplete(void *user_data)
{
	accelAct_t *aa = (accelAct_t *) user_data;
	aa->sendFunc(aa);
}

void sendToAccel(accelAct_t *aa,
				 char *data,
				 uint8_t len,
				 AccelSendCompleteFunc f)
{
	aa->sendFunc = f;
	hal_twi_send(ACCEL_ADDR, data, len, _accelSendComplete, aa);
}


/*** receiving a packet from the accelerometer ***/

void _accelReadComplete(TWIRecvSlot *trs, uint8_t len)
{
	accelAct_t *aa = (accelAct_t *) trs->user_data;
	aa->recvFunc(aa, trs->data, aa->readLen);
}

void _readFromAccel2(accelAct_t *aa)
{
	aa->trs = (TWIRecvSlot *) aa->TWIrecvBuf;
	aa->trs->func = _accelReadComplete;
	aa->trs->capacity = aa->readLen;
	aa->trs->user_data = aa;
	hal_twi_read(ACCEL_ADDR, aa->trs);
}


void readFromAccel(accelAct_t *aa,
				   uint16_t baseAddr,
				   int len,
				   AccelRecvCompleteFunc f)
{
	aa->recvFunc = f;
	aa->readLen = len;

	aa->TWIsendBuf[0] = baseAddr;
	sendToAccel(aa, aa->TWIsendBuf, 1, _readFromAccel2);
}



/*************************************/

// Configuring the accelerometer means first reading config byte 0x14,
// then setting the low bits (leaving the 3 highest bits untouched),
// and writing it back.

void configAccel3(accelAct_t *aa)
{
	schedule_now((Activation *) aa);
}


void configAccel2(accelAct_t *aa, char *data, int len)
{
	aa->TWIsendBuf[0] = 0x14;
	aa->TWIsendBuf[1] = (data[0] & 0b11100000) | (RANGE_BITS << 3) | (SAMPLING_RATE_BITS);
	sendToAccel(aa, aa->TWIsendBuf, 2, configAccel3);
}


void configAccel(accelAct_t *aa)
{
	readFromAccel(aa, 0x14, 1, configAccel2);
}


/*************************************/


static inline int16_t convert_accel(char lsb, char msb)
{
	int16_t mag = ((((int16_t) msb) & 0b01111111) << 2) | ((lsb & 0b11000000) >> 6);
	
	if (msb & 0b10000000)
		mag = mag-512;

	return mag;
}

void sampleAccelDone(accelAct_t *aa, char *data, int len)
{
	// convert and send the data out of serial
	int16_t x = convert_accel(data[0], data[1]);
	int16_t y = convert_accel(data[2], data[3]);
	int16_t z = convert_accel(data[4], data[5]);
	snprintf(aa->UARTsendBuf, sizeof(aa->UARTsendBuf)-1, "^a;x=%5d;y=%5d;z=%5d$\r\n", x, y, z);

	uart_send(RULOS_UART0, aa->UARTsendBuf, strlen(aa->UARTsendBuf), NULL, NULL);
}

void sampleAccel(accelAct_t *aa)
{
	// schedule reading of the next sample
	schedule_us(50000, (Activation *) aa);

	// read 6 bytes starting from address 0x2
	readFromAccel(aa, 0x2, 6, sampleAccelDone);
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
	aa.f = (ActivationFunc) sampleAccel;

	configAccel(&aa);

	CpumonAct cpumon;
	cpumon_init(&cpumon);
	cpumon_main_loop();
	assert(FALSE);


	return 0;
}
