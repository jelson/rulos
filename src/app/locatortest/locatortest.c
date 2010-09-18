//#define TIME_DEBUG
//#define MANUAL_US_TEST
//#define NO_ACCEL
#define NO_GYRO
//#define NO_ANALOG

#include <string.h>
#include <avr/interrupt.h>

#define F_CPU 8000000UL
#include <util/delay.h>

#include "rocket.h"
#include "hardware.h"

struct locatorAct;
typedef struct locatorAct locatorAct_t;

typedef void (*PeripheralSendCompleteFunc)(locatorAct_t *aa);
typedef void (*PeripheralRecvCompleteFunc)(locatorAct_t *aa, char *data, int len);

#ifndef NO_ANALOG
static inline void chirp(uint16_t chirpLenUS);
#endif



struct locatorAct {
	ActivationFunc f;

	// uart
	char UARTsendBuf[48];
	uint8_t uartSending;

	// twi
	char TWIsendBuf[10];
	char TWIrecvBuf[10];
	TWIRecvSlot *trs;
	PeripheralSendCompleteFunc sendFunc;
	PeripheralRecvCompleteFunc recvFunc;
	uint8_t twiAddr;
	uint8_t readLen;

#ifdef MANUAL_US_TEST
	uint8_t currUsState;
	uint8_t currUsCount;
#endif

	// ultrasound
	char rangingMode;
	uint16_t trainingMode;
	uint16_t noiseMin, noiseMax;
	uint16_t threshMin, threshMax;
	uint16_t runLength;
	Time chirpSendTime;
	uint8_t chirpTimeout;
};

static locatorAct_t aa_g;


#define ACCEL_ADDR 0b0111000
#define ACCEL_SAMPLING_RATE_BITS 0b001 // 50 hz
#define ACCEL_RANGE_BITS 0b01 // +/- 4g

#define GYRO_ADDR  0b1101000
#define GYRO_FS_SEL 0x3 // 2000 deg/sec
#define GYRO_DLPF_CFG 0x3 // 42 hz

#define FIRMWARE_ID "$Rev$"
#define ID_OFFSET 6

#define GPIO_10V_ENABLE GPIO_B0
#define GPIO_US_XMIT    GPIO_D6
#define GPIO_US_RECV    GPIO_C6
#define US_RECV_CHANNEL 0
#define US_QUIET_TRAINING_LEN 2000
#define US_MIN_NOISE_RANGE    25
#define US_MIN_RUN_LENGTH     5
#define US_CHIRP_LEN_US       1000
#define US_CHIRP_RING_TIME_MS 40

/****************************************************/


/***** serial port helpers ******/

static inline void uartSendDone(void *cb_data)
{
	locatorAct_t *aa = (locatorAct_t *) cb_data;
	aa->uartSending = FALSE;
}

static inline void emit(locatorAct_t *aa, char *s)
{
	aa->uartSending = TRUE;
	uart_send(RULOS_UART0, s, strlen(s), uartSendDone, aa);
}

static inline uint8_t maybe_claim_serial(locatorAct_t *aa)
{
	uint8_t retval = FALSE;
	cli(); // disable interrupts
	if (aa->uartSending == FALSE) {
		aa->uartSending = TRUE;
		retval = TRUE;
	}
	sei();
	return retval;
}



/**** time-related debugging ****/

#ifdef TIME_DEBUG

#define MAX_TIME_RECORDS 30
Time timerecord[MAX_TIME_RECORDS];
uint8_t numTimeRecords = 0;

void recordTime(Time t)
{
	if (numTimeRecords < MAX_TIME_RECORDS) {
		timerecord[numTimeRecords++] = t;
	}
}
void printTimeRecords(char *prefix)
{
	uint8_t i;

	for (i = 0; i < numTimeRecords; i++) {
		snprintf(aa_g.UARTsendBuf, sizeof(aa_g.UARTsendBuf)-1,
				 "%s %d: %ld\r\n",
				 prefix, i, timerecord[i]);
		emit(&aa_g, aa_g.UARTsendBuf);
	}
}
#endif


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


///////////// Ultrasound ////////////////////////////////////

#ifndef NO_ANALOG

void start_sampling()
{
	gpio_make_input_no_pullup(GPIO_US_RECV);

	// At 8mhz, an ADC prescaler of 32 gives an ADC clock of 250 khz.
	// At 13 cycles per conversion, we're sampling at 19.2 khz.

	ADCSRA = 
		_BV(ADEN) | // enable ADC
		_BV(ADATE) | // auto-trigger mode enable
		_BV(ADIE)  | // enable interrupts
		_BV(ADPS2) | _BV(ADPS1) // prescaler: 64
		;

	// configure adc channel, and also set other bits in the register
	// to 0 (which uses aref, and sets adlar to 0)
	ADMUX = US_RECV_CHANNEL;

	// set to 'free running' mode
	ADCSRB = 0;

	// start conversions
	reg_set(&ADCSRA, ADSC);
}

void stop_sampling()
{
	ADCSRA = 0;
}

void updateNoiseThresholds()
{
	uint16_t range = aa_g.noiseMax - aa_g.noiseMin;

	// in case it happens to be very not-noisy when we train, enforce
	// a minimum noise band
	if (range < US_MIN_NOISE_RANGE)
		range = US_MIN_NOISE_RANGE;
	
	aa_g.threshMin = aa_g.noiseMin - (range/2);
	aa_g.threshMax = aa_g.noiseMax + (range/2);

	if (maybe_claim_serial(&aa_g)) {
		snprintf(aa_g.UARTsendBuf, sizeof(aa_g.UARTsendBuf)-1, "^t;l=%5d;h=%5d$\r\n",
				 aa_g.noiseMin, aa_g.noiseMax);
		emit(&aa_g, aa_g.UARTsendBuf);
	}
}


// Called when the ADC has a sample ready for us.
ISR(ADC_vect)
{
	uint16_t adcval = ADCL;
	adcval |= ((uint16_t) ADCH << 8);

	// If we're in training mode, see if the current ADC value is
	// lower or higher than the previously computed noise threshold.
	if (aa_g.trainingMode) {
		aa_g.trainingMode--;
		if (aa_g.noiseMin == 0 && aa_g.noiseMax == 0) {
			aa_g.noiseMin = aa_g.noiseMax = adcval;
			updateNoiseThresholds();
		} else {
			if (adcval < aa_g.noiseMin) {
				aa_g.noiseMin = adcval;
				updateNoiseThresholds();
			}
			if (adcval > aa_g.noiseMax) {
				aa_g.noiseMax = adcval;
				updateNoiseThresholds();
			}
		}
		return;
	}

	// If we're not in training mode, see if the current ADC value is
	// beyond the noise threshold.  If so, it's a chirp!
	if (adcval < aa_g.threshMin || adcval > aa_g.threshMax)
		aa_g.runLength++;
	else
		aa_g.runLength = 0;

	if (aa_g.runLength > US_MIN_RUN_LENGTH) {
		// US detected!

		// reset the run-length detector
		aa_g.runLength = 0;

		// If we're in reflector mode, wait 500usec, then chirp back.
		if (aa_g.rangingMode == 'r') {
			_delay_ms(US_CHIRP_RING_TIME_MS + 10);
			chirp(US_CHIRP_LEN_US);
			_delay_ms(US_CHIRP_RING_TIME_MS); // make sure we don't reply to our own ping
			if (maybe_claim_serial(&aa_g)) {
				snprintf(aa_g.UARTsendBuf, sizeof(aa_g.UARTsendBuf)-1,
						 "^m;Sent chirp reply %ld$\n\r", precise_clock_time_us());
				emit(&aa_g, aa_g.UARTsendBuf);
			}
		}

		// If we're in sender mode, and we got a chirp, report it and
		// exit sender mode
		if (aa_g.rangingMode == 's') {
			aa_g.rangingMode = 'q';
			Time end = precise_clock_time_us();
			int32_t diff = end - aa_g.chirpSendTime;

			if (maybe_claim_serial(&aa_g)) {
				snprintf(aa_g.UARTsendBuf, sizeof(aa_g.UARTsendBuf)-1,
						 //"^d;s=%ld,e=%ld,t=%ld$\r\n",
						 "^d;t=%ld$\r\n",
						 //aa_g.chirpSendTime, end,
						 diff);
				emit(&aa_g, aa_g.UARTsendBuf);
			}
		}
	}
}



static inline void us_xmit_start()
{
	// We are using "fast pwm" mode, i.e., mode 7 (0b111), where TOP=OCR0A.

	// Furthermore since the on and off intervals are the same, we'll use 

	// atmega manual p.107: to have OC0A toggled on each compare match, set
	//    COM0A[1:0] = 0b01
	// running at 8mhz, getting a 40khz clock means switching every 200 cycles (prescaler=1)

	gpio_make_input(GPIO_US_XMIT);
	TCCR0A = _BV(COM0A0) | _BV(WGM01) | _BV(WGM00);
	TCCR0B = _BV(WGM02) | _BV(CS00);
	OCR0A = 100;
	TCNT0 = 0;
	gpio_make_output(GPIO_US_XMIT);
}

static inline void us_xmit_stop()
{
	TCCR0A = 0;
}

static inline void chirp(uint16_t chirpLenUS)
{
	us_xmit_start();
	_delay_us(chirpLenUS);
	us_xmit_stop();
}

#endif

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
#ifdef NO_GYRO
	configLocator4(aa);
	return;
#endif

	aa->TWIsendBuf[0] = 0x16;
	aa->TWIsendBuf[1] =
		(GYRO_FS_SEL << 3) |
		(GYRO_DLPF_CFG);
	sendToPeripheral(aa, GYRO_ADDR, aa->TWIsendBuf, 2, configLocator4);
}


// Configure the accelerometer by first reading config byte 0x14, then
// setting the low bits (leaving the 3 highest bits untouched), and
// writing it back.  This callback is called when the initial read
// completes.
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
#ifndef NO_ANALOG
	// turn off the us output switch
	gpio_make_output(GPIO_US_XMIT);
	gpio_clr(GPIO_US_XMIT);

	// enable 10v voltage doubler
	gpio_make_output(GPIO_10V_ENABLE);
	gpio_clr(GPIO_10V_ENABLE);

	aa->trainingMode = US_QUIET_TRAINING_LEN;
	start_sampling();
#endif

#ifdef NO_ACCEL
	configLocator3(aa);
	return;
#endif

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

#ifndef NO_GYRO
void sampleLocator3(locatorAct_t *aa, char *data, int len)
{
	// convert and send the data out of serial
	int16_t x = ((int16_t) data[0]) << 8 | data[1];
	int16_t y = ((int16_t) data[2]) << 8 | data[3];
	int16_t z = ((int16_t) data[4]) << 8 | data[5];

	if (maybe_claim_serial(aa)) {
		snprintf(aa->UARTsendBuf, sizeof(aa->UARTsendBuf)-1, "^g;x=%5d;y=%5d;z=%5d$\r\n", x, y, z);
		emit(aa, aa->UARTsendBuf);
	}
}
#endif


void sampleLocator2(locatorAct_t *aa, char *data, int len)
{
#ifndef NO_GYRO
	// read 6 bytes from gyro over TWI starting from address 0x2
	readFromPeripheral(aa, GYRO_ADDR, 0x1D, 6, sampleLocator3);
#endif

#ifndef NO_ACCEL
	// convert and send the accelerometer data we just received out to
	// the serial port
	int16_t x = convert_accel(data[0], data[1]);
	int16_t y = convert_accel(data[2], data[3]);
	int16_t z = convert_accel(data[4], data[5]);

	if (maybe_claim_serial(aa)) {
		snprintf(aa->UARTsendBuf, sizeof(aa->UARTsendBuf)-1, "^a;x=%5d;y=%5d;z=%5d$\r\n", x, y, z);
		emit(aa, aa->UARTsendBuf);
	}
#endif
}
	
	
void sampleLocator(locatorAct_t *aa)
{
	// schedule reading of the next sample
	schedule_us(50000, (Activation *) aa);

	cli();
	if (aa->rangingMode == 's') {
		aa->chirpTimeout++;

		if (aa->chirpTimeout >= 3) {
			aa->rangingMode = 'q';
			emit(aa, "^m;ranging timeout$\n\r");
		}
	}
	sei();

	// see if we've received any serial commands
	char cmd;
	while (CharQueue_pop(uart_recvq(RULOS_UART0)->q, &cmd)) {
		switch (cmd) {
#ifndef NO_ANALOG
		case 'q':
			aa->rangingMode = 'q';
			us_xmit_stop();
			emit(aa, "^m;entering quiet mode$\n\r");
			break;

		case 'c':
			us_xmit_start();
			break;

		case 's':
			aa->rangingMode = 'q';
			emit(aa, "^m;chirping$\n\r");
			chirp(US_CHIRP_LEN_US);
			_delay_ms(US_CHIRP_RING_TIME_MS);
			aa->chirpSendTime = precise_clock_time_us();
			aa->chirpTimeout = 0;
			aa->rangingMode = 's';
			return; // don't sample the periphs this time around
			break;

		case 'r':
			aa->rangingMode = 'r';
			emit(aa, "^m;entering reflector mode$\n\r");
			break;
#endif
		}
	}

#ifdef MANUAL_US_TEST
	// maybe flip the us
	aa->currUsCount++;
	if (aa->currUsCount >= 70) {
		aa->currUsState = !aa->currUsState;
		if (aa->currUsState)
			emit(aa, "on\n\r");
		else
			emit(aa, "off\n\r");
		gpio_set_or_clr(GPIO_US_XMIT, aa->currUsState);
		aa->currUsCount = 0;
	}
#endif

#ifdef NO_ACCEL
	sampleLocator2(aa, NULL, 0);
	return;
#endif

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
	uart_init(RULOS_UART0, 1); // 250kbps when processor is at 8mhz

	memset(&aa_g, 0, sizeof(aa_g));
	aa_g.f = (ActivationFunc) sampleLocator;

	if (strlen(FIRMWARE_ID) > ID_OFFSET) {
		snprintf(aa_g.UARTsendBuf, sizeof(aa_g.UARTsendBuf)-1, "^i;%s\r\n", FIRMWARE_ID+ID_OFFSET);
	} else {
		snprintf(aa_g.UARTsendBuf, sizeof(aa_g.UARTsendBuf)-1, "^i;0$\r\n");
	}
	emit(&aa_g, aa_g.UARTsendBuf);

#ifdef TIME_DEBUG
	for (uint8_t i = 0; i < 10; i++)
		recordTime(precise_clock_time_us());
	printTimeRecords("res");
#endif

	configLocator(&aa_g);

	CpumonAct cpumon;
	cpumon_init(&cpumon);
	cpumon_main_loop();
	assert(FALSE);

	return 0;
}
