#include "rocket.h"

struct locatorAct;
typedef struct locatorAct locatorAct_t;

typedef void (*PeripheralSendCompleteFunc)(locatorAct_t *aa);
typedef void (*PeripheralRecvCompleteFunc)(locatorAct_t *aa, char *data, int len);


struct locatorAct {
	ActivationFunc f;
	char TWIsendBuf[20];
	char TWIrecvBuf[5];
	char UARTsendBuf[35];
	TWIRecvSlot *trs;
	PeripheralSendCompleteFunc sendFunc;
	PeripheralRecvCompleteFunc recvFunc;
	uint8_t twiAddr;
	uint8_t readLen;
};

#define ACCEL_ADDR 0b0111000
#define ACCEL_SAMPLING_RATE_BITS 0b001 // 50 hz
#define ACCEL_RANGE_BITS 0b01 // +/- 4g

#define GYRO_ADDR  0b1101000
#define GYRO_FS_SEL 0x3 // 2000 deg/sec
#define GYRO_DLPF_CFG 0x3 // 42 hz

#define FIRMWARE_ID "$Rev$"
#define ID_OFFSET 6

/****************************************************/


/*** sending data to a locator peripheral ***/

void _peripheralSendComplete(void *user_data)
{
	locatorAct_t *aa = (locatorAct_t *) user_data;
	aa->sendFunc(aa);
}

void sendToPeripheral(locatorAct_t *aa,
					  uint8_t twiAddr,
					  char *data,
					  uint8_t len,
					  PeripheralSendCompleteFunc f)
{
	aa->sendFunc = f;
	hal_twi_send(twiAddr, data, len, _peripheralSendComplete, aa);
}


/*** receiving data from a locator peripheral, using the
	 write-address-then-read protocol ***/

void _locatorReadComplete(TWIRecvSlot *trs, uint8_t len)
{
	locatorAct_t *aa = (locatorAct_t *) trs->user_data;
	aa->recvFunc(aa, trs->data, aa->readLen);
}

void _readFromPeripheral2(locatorAct_t *aa)
{
	aa->trs = (TWIRecvSlot *) aa->TWIrecvBuf;
	aa->trs->func = _locatorReadComplete;
	aa->trs->capacity = aa->readLen;
	aa->trs->user_data = aa;
	hal_twi_read(aa->twiAddr, aa->trs);
}


void readFromPeripheral(locatorAct_t *aa,
						uint8_t twiAddr,
						uint16_t baseAddr,
						int len,
						PeripheralRecvCompleteFunc f)
{
	aa->recvFunc = f;
	aa->readLen = len;

	aa->twiAddr = twiAddr;
	aa->TWIsendBuf[0] = baseAddr;
	sendToPeripheral(aa, twiAddr, aa->TWIsendBuf, 1, _readFromPeripheral2);
}



/*************************************/

// all done!  start sampling.
void configLocator4(locatorAct_t *aa)
{
	schedule_now((Activation *) aa);
}


// Next configure the gyro by writing the full-scale deflection and
// sample-rate bits to register 0x16.
void configLocator3(locatorAct_t *aa)
{
	aa->TWIsendBuf[0] = 0x16;
	aa->TWIsendBuf[1] =
		(GYRO_FS_SEL << 3) |
		(GYRO_DLPF_CFG);
	sendToPeripheral(aa, GYRO_ADDR, aa->TWIsendBuf, 2, configLocator4);
}


// Configure the accelerometer by first reading config byte 0x14,
// then setting the low bits (leaving the 3 highest bits untouched),
// and writing it back.
void configLocator2(locatorAct_t *aa, char *data, int len)
{
	aa->TWIsendBuf[0] = 0x14;
	aa->TWIsendBuf[1] =
		(data[0] & 0b11100000) |
		(ACCEL_RANGE_BITS << 3) |
		(ACCEL_SAMPLING_RATE_BITS);
	sendToPeripheral(aa, ACCEL_ADDR, aa->TWIsendBuf, 2, configLocator3);
}


void configLocator(locatorAct_t *aa)
{
	readFromPeripheral(aa, ACCEL_ADDR, 0x14, 1, configLocator2);
}


/*************************************/


static inline int16_t convert_accel(char lsb, char msb)
{
	int16_t mag = ((((int16_t) msb) & 0b01111111) << 2) | ((lsb & 0b11000000) >> 6);
	
	if (msb & 0b10000000)
		mag = mag-512;

	return mag;
}

void sampleLocator3(locatorAct_t *aa, char *data, int len)
{
	// convert and send the data out of serial
	int16_t x = ((int16_t) data[0]) << 8 | data[1];
	int16_t y = ((int16_t) data[2]) << 8 | data[3];
	int16_t z = ((int16_t) data[4]) << 8 | data[5];

	snprintf(aa->UARTsendBuf, sizeof(aa->UARTsendBuf)-1, "^g;x=%5d;y=%5d;z=%5d$\r\n", x, y, z);
	uart_send(RULOS_UART0, aa->UARTsendBuf, strlen(aa->UARTsendBuf), NULL, NULL);
}

void sampleLocator2(locatorAct_t *aa, char *data, int len)
{
	// read 6 bytes from gyro starting from address 0x2
	readFromPeripheral(aa, GYRO_ADDR, 0x1D, 6, sampleLocator3);

	// convert and send the data out of serial
	int16_t x = convert_accel(data[0], data[1]);
	int16_t y = convert_accel(data[2], data[3]);
	int16_t z = convert_accel(data[4], data[5]);

	snprintf(aa->UARTsendBuf, sizeof(aa->UARTsendBuf)-1, "^a;x=%5d;y=%5d;z=%5d$\r\n", x, y, z);
	uart_send(RULOS_UART0, aa->UARTsendBuf, strlen(aa->UARTsendBuf), NULL, NULL);
}

void sampleLocator(locatorAct_t *aa)
{
	// schedule reading of the next sample
	schedule_us(50000, (Activation *) aa);

	// read 6 bytes from accelerometer starting from address 0x2
	readFromPeripheral(aa, ACCEL_ADDR, 0x2, 6, sampleLocator2);
}




int main()
{
	heap_init();
	util_init();
	hal_init(bc_audioboard);
	init_clock(10000, TIMER1);

	hal_twi_init(0, NULL);
	uart_init(RULOS_UART0, 0);

	locatorAct_t aa;
	aa.f = (ActivationFunc) sampleLocator;

	configLocator(&aa);

	if (strlen(FIRMWARE_ID) > ID_OFFSET) {
		snprintf(aa.UARTsendBuf, sizeof(aa.UARTsendBuf)-1, "^i;%s\r\n", FIRMWARE_ID+ID_OFFSET);
	} else {
		snprintf(aa.UARTsendBuf, sizeof(aa.UARTsendBuf)-1, "^i;0$\r\n");
	}
	uart_send(RULOS_UART0, aa.UARTsendBuf, strlen(aa.UARTsendBuf), NULL, NULL);

	CpumonAct cpumon;
	cpumon_init(&cpumon);
	cpumon_main_loop();
	assert(FALSE);

	return 0;
}
